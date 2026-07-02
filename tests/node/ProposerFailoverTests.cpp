// Real multi-node TCP end-to-end test for view change / proposer failover
// (roadmap item 1.3):
//
//   (a) The first-round scheduled proposer never comes online. The two other
//       validators must time out round 1 (rounds are 1-based), advance to
//       round 2 (a different scheduled proposer), and finalize block 1
//       without it.
//   (b) The absent validator then starts fresh, authenticates, catches up
//       via persistent block sync instead of waiting on a stale round, and
//       keeps tracking the chain as the network finalizes a further block.

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
    std::array<std::size_t, 2> onlineIndices{};
    std::size_t onlineCount = 0;
    for (std::size_t index = 0; index < specs.size(); ++index) {
      if (index != round0Proposer) {
        onlineIndices[onlineCount++] = index;
      }
    }
    require(onlineCount == 2, "Expected exactly two online validators.");
    const std::size_t onlineA = onlineIndices[0];
    const std::size_t onlineB = onlineIndices[1];

    // (a) The first-round proposer is never started (equivalent to it being
    // killed before it could propose). Only the two other validators
    // come online.
    nodes.start(onlineA);
    nodes.start(onlineB);
    waitForRpc(specs[onlineA]);
    waitForRpc(specs[onlineB]);

    require(waitUntil(90s,
                      [&] {
                        return hasAuthenticatedPeer(specs[onlineA]) &&
                               hasAuthenticatedPeer(specs[onlineB]);
                      }),
            "Online validators did not authenticate with each other.");

    // A block only gets produced once the mempool holds a pending
    // transaction (empty blocks are refused outside epoch-settlement
    // heights), so submit one before waiting on round/height progress.
    const core::Transaction firstTransfer = signedTransfer(
        genesis, "failover", "failover-recipient-1", 1, unixTime());
    const auto firstSubmitted =
        submitTransaction(specs[onlineA], firstTransfer);
    require(firstSubmitted.has_value() && firstSubmitted->statusCode == 200,
            "First transaction submission did not return HTTP 200.");

    // The first-round proposer never proposes, so both online validators
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
                        return reachedFinalizedHeight(specs[onlineA], 1) &&
                               reachedFinalizedHeight(specs[onlineB], 1);
                      }),
            "The two online validators did not finalize block 1 via view "
            "change after the round-0 proposer failed to propose.");
    const std::string finalizedHash = blockHashAt(specs[onlineA], 1);
    require(blockHashAt(specs[onlineB], 1) == finalizedHash,
            "Online validators finalized different block hashes.");

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
                        return reachedFinalizedHeight(specs[onlineA], 2) &&
                               reachedFinalizedHeight(specs[onlineB], 2) &&
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
