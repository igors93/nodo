// Real multi-node TCP end-to-end test (roadmap item 1.4, gate scenario 2):
// a transaction submitted at node A reaches node C's mempool through relay
// gossip, even though A and C are never statically peered with each other —
// only through the intermediate node B (a line topology A-B-C). Propagation
// therefore requires B to receive, admit, and re-broadcast the transaction
// (NodeDaemon::processTransactionGossip does this for every admitted
// transaction), i.e. two hops, not a direct A-to-C broadcast. The
// transaction must then be included in a finalized block on all three
// nodes.

#include "../common/RealTcpNodeTestSupport.hpp"

namespace {

#ifndef _WIN32

void testTwoHopTransactionPropagation() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-tx-propagation-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const NodeSpecs specs = makeNodeSpecs(root, "txprop");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "txprop");

  // Line topology: A(0) -- B(1) -- C(2). Neither A nor C statically dials
  // the other; B is the only node that dials anyone from A's side.
  Topology topology;
  topology[1] = {0}; // B dials A
  topology[2] = {1}; // C dials B
  ChildProcesses nodes(specs, genesis, topology);

  const NodeSpec &nodeA = specs[0];
  const NodeSpec &nodeB = specs[1];
  const NodeSpec &nodeC = specs[2];

  try {
    nodes.startAll();
    waitForRpc(nodeA);
    waitForRpc(nodeB);
    waitForRpc(nodeC);

    // 120s, not 90s: matches the more generous budget already used for
    // comparable authenticated-peer waits elsewhere in this test family
    // (e.g. ProposerFailoverTests' post-recovery wait) to absorb slower
    // process-startup/handshake timing on loaded CI runners.
    require(waitUntil(120s,
                      [&] {
                        return authenticatedPeerCount(nodeA) == 1 &&
                               authenticatedPeerCount(nodeB) == 2 &&
                               authenticatedPeerCount(nodeC) == 1;
                      }),
            "Nodes did not form the expected line topology (A-B-C, no "
            "direct A-C edge). A: " +
                std::to_string(authenticatedPeerCount(nodeA)) +
                " B: " + std::to_string(authenticatedPeerCount(nodeB)) +
                " C: " + std::to_string(authenticatedPeerCount(nodeC)));

    const core::Transaction transaction =
        signedTransfer(genesis, "txprop", "txprop-recipient", 1, unixTime());
    const auto submitted = submitTransaction(nodeA, transaction);
    require(submitted.has_value() && submitted->statusCode == 200,
            "RPC transaction submission at node A did not return HTTP 200.");
    require(submitted->body.find("\"status\":\"ACCEPTED\"") !=
                std::string::npos,
            "RPC transaction submission was not accepted: " + submitted->body);

    // C only has a path to A's transaction through B's relay
    // (processTransactionGossip re-broadcasts every admitted tx to its
    // own peers), so this can only succeed as a two-hop propagation.
    require(waitUntil(90s,
                      [&] {
                        return mempoolSize(nodeC) >= 1 ||
                               reachedFinalizedHeight(nodeC, 1);
                      }),
            "The transaction submitted at node A did not propagate through "
            "node B to node C's mempool.");

    require(waitUntil(120s,
                      [&] {
                        return reachedFinalizedHeight(nodeA, 1) &&
                               reachedFinalizedHeight(nodeB, 1) &&
                               reachedFinalizedHeight(nodeC, 1);
                      }),
            "The relayed transaction was not finalized on all three nodes.");

    const std::string finalizedHash = blockHashAt(nodeA, 1);
    require(blockHashAt(nodeB, 1) == finalizedHash &&
                blockHashAt(nodeC, 1) == finalizedHash,
            "Nodes finalized different block hashes.");

    verifyTransactionFinalized(nodeA, transaction.id(), 1);
    verifyTransactionFinalized(nodeB, transaction.id(), 1);
    verifyTransactionFinalized(nodeC, transaction.id(), 1);

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
  std::cout
      << "Transaction propagation E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testTwoHopTransactionPropagation();
    std::cout << "Transaction propagation end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Transaction propagation end-to-end test FAILED: "
              << error.what() << '\n';
    return 1;
  }
#endif
}
