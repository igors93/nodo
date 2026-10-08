// Real multi-node TCP end-to-end test for view change / proposer failover
// (roadmap item 1.3):
//
//   (a) The first-round scheduled proposer never comes online. The three
//       other validators must time out round 1 (rounds are 1-based), advance
//       to a later round with a different scheduled proposer, and finalize
//       block 1 without it.
//   (b) The absent validator then starts fresh, authenticates, catches up
//       via persistent block sync instead of waiting on a stale round, and
//       keeps tracking the chain as the network finalizes a further block.
//
// Four equal-weight validators: the strict quorum floor(2W/3)+1 (ADR 0002)
// tolerates one absent validator only when W >= 4. With three, all three
// must vote and a single absent proposer halts finality.
#define NODO_REAL_TCP_NODE_COUNT 4
#include "../common/RealTcpNodeTestSupport.hpp"

namespace {

#ifndef _WIN32

void testProposerFailoverAndLaggingNodeRecovery() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-proposer-failover-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const NodeSpecs specs = makeNodeSpecs(root, "failover");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "failover");
  const Topology topology = fullMeshTopology();
  ChildProcesses nodes(specs, genesis, topology);

  try {
    const std::size_t round0Proposer = scheduledProposerIndex(specs, genesis);
    std::array<std::size_t, kTestNodeCount - 1> onlineIndices{};
    std::size_t onlineCount = 0;
    for (std::size_t index = 0; index < specs.size(); ++index) {
      if (index != round0Proposer) {
        onlineIndices[onlineCount++] = index;
      }
    }
    require(onlineCount == onlineIndices.size(),
            "Expected every validator except the round-1 proposer online.");
    const auto allOnline = [&](const auto &predicate) {
      for (const std::size_t index : onlineIndices) {
        if (!predicate(specs[index])) {
          return false;
        }
      }
      return true;
    };

    // (a) The first-round proposer is never started (equivalent to it being
    // killed before it could propose). Only the other validators come
    // online.
    for (const std::size_t index : onlineIndices) {
      nodes.start(index);
    }
    for (const std::size_t index : onlineIndices) {
      waitForRpc(specs[index]);
    }

    // Votes are not relayed, so every online validator needs a direct
    // session with each other online validator to see a quorum.
    require(waitUntil(90s,
                      [&] {
                        return allOnline([&](const NodeSpec &spec) {
                          return authenticatedPeerCount(spec) >=
                                 onlineIndices.size() - 1;
                        });
                      }),
            "Online validators did not authenticate with each other.");
    const std::size_t onlineA = onlineIndices[0];

    // A block only gets produced once the mempool holds a pending
    // transaction (empty blocks are refused outside epoch-settlement
    // heights), so submit one before waiting on round/height progress.
    const core::Transaction firstTransfer = signedTransfer(
        genesis, "failover", "failover-recipient-1", 1, unixTime());
    const auto firstSubmitted =
        submitTransaction(specs[onlineA], firstTransfer);
    require(firstSubmitted.has_value() && firstSubmitted->statusCode == 200,
            "First transaction submission did not return HTTP 200.");

    // The first-round proposer never proposes, so the online validators
    // must time out and advance past round 1 (rounds are 1-based) before
    // any block can be finalized. Catching round > 1 here proves the view
    // change fired rather than the block having been finalized by some
    // other means.
    require(waitUntil(90s,
                      [&] {
                        return currentRound(specs[onlineA]) >= 2 ||
                               reachedFinalizedHeight(specs[onlineA], 1);
                      }),
            "Round did not advance past round 1 despite the scheduled "
            "proposer being absent.");

    require(waitUntil(180s,
                      [&] {
                        return allOnline([](const NodeSpec &spec) {
                          return reachedFinalizedHeight(spec, 1);
                        });
                      }),
            "The online validators did not finalize block 1 via view "
            "change after the round-1 proposer failed to propose.");
    const std::string finalizedHash = blockHashAt(specs[onlineA], 1);
    for (const std::size_t index : onlineIndices) {
      require(blockHashAt(specs[index], 1) == finalizedHash,
              "Online validators finalized different block hashes.");
    }

    // (b) The previously-absent round-0 proposer now starts, fresh,
    // with an empty chain. It must authenticate, discover the height
    // gap, and recover via persistent block sync instead of stalling
    // while waiting for a proposal at its stale height.
    nodes.start(round0Proposer);
    waitForRpc(specs[round0Proposer]);
    require(waitUntil(120s,
                      [&] {
                        return hasAuthenticatedPeer(specs[round0Proposer]) &&
                               reachedFinalizedHeight(specs[round0Proposer], 1);
                      }),
            "The previously-absent validator did not authenticate and "
            "synchronize block 1 via block sync.");
    require(blockHashAt(specs[round0Proposer], 1) == finalizedHash,
            "The recovered validator synchronized a different block "
            "hash than the online validators finalized.");

    // The recovered node must not just be a passive observer: the
    // network as a whole (including it) must keep advancing.
    const core::Transaction secondTransfer = signedTransfer(
        genesis, "failover", "failover-recipient-2", 2, unixTime());
    const auto submitted = submitTransaction(specs[onlineA], secondTransfer);
    require(submitted.has_value() && submitted->statusCode == 200,
            "Second transaction submission did not return HTTP 200.");

    require(waitUntil(180s,
                      [&] {
                        return allOnline([](const NodeSpec &spec) {
                                 return reachedFinalizedHeight(spec, 2);
                               }) &&
                               reachedFinalizedHeight(specs[round0Proposer], 2);
                      }),
            "The network (including the recovered validator) did not "
            "advance to block 2 after recovery.");

    nodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    nodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
    throw;
  }
}

#endif

} // namespace

int main() {
#ifdef _WIN32
  std::cout << "Proposer failover E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testProposerFailoverAndLaggingNodeRecovery();
    std::cout << "Proposer failover end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Proposer failover end-to-end test FAILED: " << error.what()
              << '\n';
    return 1;
  }
#endif
}
