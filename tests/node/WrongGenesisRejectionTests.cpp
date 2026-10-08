// Real multi-node TCP end-to-end test: a peer whose genesis configuration
// diverges from the network's is rejected at handshake, while unaffected
// honest peers keep authenticating with each other normally.
//
// Three honest validators (index 0, 2 and 3) run the shared "honest"
// genesis. A fourth process (index 1) runs with a differently-memoed genesis
// (different deterministic genesis id, same network parameters/chain id),
// reusing the same validator identity/ports slot the honest network reserved
// for it. The static-peer topology is a full mesh, so:
//   - nodes 0, 2 and 3 (all honest) must authenticate with each other.
//   - every honest node <-> node 1 (impostor) must be rejected.
//
// Four equal-weight validators: the strict quorum floor(2W/3)+1 (ADR 0002)
// lets the three honest validators finalize without the rejected one. With
// three validators the honest pair would hold exactly 2/3, which is not a
// quorum.
#define NODO_REAL_TCP_NODE_COUNT 4
#include "../common/RealTcpNodeTestSupport.hpp"

namespace {

#ifndef _WIN32

void testWrongGenesisPeerIsRejected() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-wrong-genesis-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const NodeSpecs specs = makeNodeSpecs(root, "wronggenesis");
  const std::int64_t genesisTimestamp = unixTime() - 5;
  const config::GenesisConfig honestGenesis =
      makeGenesis(specs, genesisTimestamp, "wronggenesis-honest");
  const config::GenesisConfig mismatchedGenesis =
      makeGenesis(specs, genesisTimestamp, "wronggenesis-impostor");
  require(honestGenesis.deterministicId() !=
              mismatchedGenesis.deterministicId(),
          "Test setup error: honest and mismatched genesis must differ.");

  const Topology topology = fullMeshTopology();
  ChildProcesses honestNodes(specs, honestGenesis, topology);
  pid_t impostorPid = 0;
  constexpr std::size_t kImpostorIndex = 1;
  const std::array<std::size_t, kTestNodeCount - 1> honestIndices{0, 2, 3};

  try {
    // Node index 1's real identity/ports slot is deliberately left out
    // of `honestNodes`: it is started separately, below, under the
    // mismatched genesis instead.
    for (const std::size_t index : honestIndices) {
      honestNodes.start(index);
    }
    impostorPid =
        forkDaemonChild(kImpostorIndex, specs, mismatchedGenesis, topology);

    for (const std::size_t index : honestIndices) {
      waitForRpc(specs[index]);
    }
    waitForRpc(specs[kImpostorIndex]);

    const std::uint64_t honestPeerCount = honestIndices.size() - 1;
    require(waitUntil(90s,
                      [&] {
                        for (const std::size_t index : honestIndices) {
                          if (authenticatedPeerCount(specs[index]) <
                              honestPeerCount) {
                            return false;
                          }
                        }
                        return true;
                      }),
            "Honest validators did not authenticate with each other.");

    // Give the impostor's reconnection policy several retry cycles: the
    // rejection must be stable, not a one-off race.
    std::this_thread::sleep_for(10s);

    require(authenticatedPeerCount(specs[kImpostorIndex]) == 0,
            "Peer with mismatched genesis must never authenticate with "
            "any honest validator.");
    for (const std::size_t index : honestIndices) {
      require(authenticatedPeerCount(specs[index]) == honestPeerCount,
              "Honest node " + std::to_string(index) +
                  " must only ever authenticate with the other honest "
                  "nodes, not with the mismatched-genesis peer.");
    }

    // The honest validators must still be able to make protocol progress
    // despite the rejected peer retrying indefinitely. A block only
    // gets produced once the mempool holds a pending transaction (empty
    // blocks are refused outside epoch-settlement heights), so submit one.
    const core::Transaction transaction =
        signedTransfer(honestGenesis, "wronggenesis-honest",
                       "wronggenesis-recipient", 1, unixTime());
    const auto submitted = submitTransaction(specs[0], transaction);
    require(submitted.has_value() && submitted->statusCode == 200,
            "Transaction submission did not return HTTP 200.");

    require(waitUntil(120s,
                      [&] {
                        for (const std::size_t index : honestIndices) {
                          if (!reachedFinalizedHeight(specs[index], 1)) {
                            return false;
                          }
                        }
                        return true;
                      }),
            "Honest validators did not finalize block 1 despite the "
            "rejected peer.");

    stopPid(impostorPid, specs[kImpostorIndex].nodeId);
    impostorPid = 0;
    honestNodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    stopPid(impostorPid, specs[kImpostorIndex].nodeId);
    honestNodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
    throw;
  }
}

#endif

} // namespace

int main() {
#ifdef _WIN32
  std::cout
      << "Wrong-genesis rejection E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testWrongGenesisPeerIsRejected();
    std::cout << "Wrong-genesis rejection end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Wrong-genesis rejection end-to-end test FAILED: "
              << error.what() << '\n';
    return 1;
  }
#endif
}
