// Real multi-node TCP end-to-end test (roadmap item 1.4, gate scenario 1):
// 3 validators finalize 10 consecutive blocks over real TCP/gossip, agree on
// every finalized hash, and each independently passes a full ChainAuditor
// pass against its own on-disk data directory once stopped.
//
// A block only gets produced when the mempool holds at least one pending
// transaction (empty blocks are refused outside epoch-settlement heights, and
// this genesis' epoch is far longer than 10 blocks), so this test submits one
// transaction per target height, waiting for each height to finalize before
// submitting the next. That keeps exactly one pending transaction available
// whenever a proposer next attempts to build a block.

#include "../common/RealTcpNodeTestSupport.hpp"

namespace {

#ifndef _WIN32

constexpr std::uint64_t kTargetFinalizedHeight = 10;

void testDistributedFinalizationOfTenBlocks() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-distributed-finalization-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const NodeSpecs specs = makeNodeSpecs(root, "distfin");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "distfin");
  const Topology topology = fullMeshTopology();
  ChildProcesses nodes(specs, genesis, topology);

  try {
    nodes.startAll();
    for (const NodeSpec &spec : specs) {
      waitForRpc(spec);
    }

    require(waitUntil(90s,
                      [&] {
                        for (const NodeSpec &spec : specs) {
                          // Full mesh of 3 nodes: each authenticates with the
                          // other 2.
                          if (authenticatedPeerCount(spec) < 2) {
                            return false;
                          }
                        }
                        return true;
                      }),
            "The three validators did not form a fully authenticated mesh.");

    for (std::uint64_t height = 1; height <= kTargetFinalizedHeight; ++height) {
      const core::Transaction transaction = signedTransfer(
          genesis, "distfin", "distfin-recipient", height, unixTime());
      const auto submitted = submitTransaction(specs[0], transaction);
      require(submitted.has_value() && submitted->statusCode == 200,
              "Transaction submission for height " + std::to_string(height) +
                  " did not return HTTP 200.");

      require(waitUntil(120s,
                        [&] {
                          for (const NodeSpec &spec : specs) {
                            if (!reachedFinalizedHeight(spec, height)) {
                              return false;
                            }
                          }
                          return true;
                        }),
              "The three validators did not finalize height " +
                  std::to_string(height) + ".");
    }

    // Consistency: every node must agree on the same hash at every
    // finalized height, not just the tip.
    for (std::uint64_t height = 1; height <= kTargetFinalizedHeight; ++height) {
      const std::string reference = blockHashAt(specs[0], height);
      for (std::size_t index = 1; index < specs.size(); ++index) {
        require(blockHashAt(specs[index], height) == reference,
                specs[index].nodeId +
                    " diverged from node 0's finalized hash at "
                    "height " +
                    std::to_string(height) + ".");
      }
    }

    // Stop cleanly (flushes state to disk) before auditing.
    nodes.stopAll();

    for (const NodeSpec &spec : specs) {
      requireChainAuditPasses(spec, genesis);
    }

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
      << "Distributed finalization E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testDistributedFinalizationOfTenBlocks();
    std::cout << "Distributed finalization end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Distributed finalization end-to-end test FAILED: "
              << error.what() << '\n';
    return 1;
  }
#endif
}
