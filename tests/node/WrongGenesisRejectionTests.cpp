// Real multi-node TCP end-to-end test: a peer whose genesis configuration
// diverges from the network's is rejected at handshake, while unaffected
// honest peers keep authenticating with each other normally.
//
// Two honest validators (index 0 and 2) run the shared "honest" genesis. A
// third process (index 1) runs with a differently-memoed genesis (different
// deterministic genesis id, same network parameters/chain id), reusing the
// same validator identity/ports slot the honest network reserved for it.
// The static-peer topology is a full mesh, so:
//   - node 0 <-> node 2 (both honest) must authenticate successfully.
//   - node 0 <-> node 1 (impostor) and node 2 <-> node 1 must be rejected.

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

  try {
    // Node index 1's real identity/ports slot is deliberately left out
    // of `honestNodes`: it is started separately, below, under the
    // mismatched genesis instead.
    honestNodes.start(0);
    honestNodes.start(2);
    impostorPid = forkDaemonChild(1, specs, mismatchedGenesis, topology);

    waitForRpc(specs[0]);
    waitForRpc(specs[2]);
    waitForRpc(specs[1]);

    require(waitUntil(90s,
                      [&] {
                        return hasAuthenticatedPeer(specs[0]) &&
                               hasAuthenticatedPeer(specs[2]);
                      }),
            "Honest validators did not authenticate with each other.");

    // Give the impostor's reconnection policy several retry cycles: the
    // rejection must be stable, not a one-off race.
    std::this_thread::sleep_for(10s);

    require(authenticatedPeerCount(specs[1]) == 0,
            "Peer with mismatched genesis must never authenticate with "
            "any honest validator.");
    require(authenticatedPeerCount(specs[0]) == 1,
            "Honest node 0 must only ever authenticate with node 2, "
            "not with the mismatched-genesis peer.");
    require(authenticatedPeerCount(specs[2]) == 1,
            "Honest node 2 must only ever authenticate with node 0, "
            "not with the mismatched-genesis peer.");

    // The honest pair must still be able to make protocol progress
    // despite the third, rejected peer retrying indefinitely. A block only
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
                        return reachedFinalizedHeight(specs[0], 1) &&
                               reachedFinalizedHeight(specs[2], 1);
                      }),
            "Honest validators did not finalize block 1 despite the "
            "rejected peer.");

    stopPid(impostorPid, specs[1].nodeId);
    impostorPid = 0;
    honestNodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    stopPid(impostorPid, specs[1].nodeId);
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
