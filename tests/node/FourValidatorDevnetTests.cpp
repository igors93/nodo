// Four separate validator processes finalize over authenticated real TCP.
#define NODO_REAL_TCP_NODE_COUNT 4
#include "../common/RealTcpNodeTestSupport.hpp"

namespace {

#ifndef _WIN32
void testFourValidatorDevnet() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-four-validator-devnet-" + std::to_string(::getpid()));
  std::error_code error;
  std::filesystem::remove_all(root, error);

  const NodeSpecs specs = makeNodeSpecs(root, "fourval");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "fourval");
  const Topology topology = fullMeshTopology();
  ChildProcesses nodes(specs, genesis, topology);

  try {
    nodes.startAll();
    for (const NodeSpec &spec : specs) waitForRpc(spec);
    require(waitUntil(90s, [&] {
              for (const NodeSpec &spec : specs) {
                if (authenticatedPeerCount(spec) < 3) return false;
              }
              return true;
            }), "Four validators did not form an authenticated TCP mesh.");

    const core::Transaction transaction =
        signedTransfer(genesis, "fourval", "recipient", 1, unixTime());
    const auto submitted = submitTransaction(specs[0], transaction);
    require(submitted.has_value() && submitted->statusCode == 200,
            "Devnet transaction submission failed.");
    require(waitUntil(120s, [&] {
              bool allFinalized = true;
              for (const NodeSpec &spec : specs) {
                allFinalized = reachedFinalizedHeight(spec, 1) && allFinalized;
              }
              return allFinalized;
            }), "Four validators did not finalize the same height.");
    const std::string hash = blockHashAt(specs[0], 1);
    for (const NodeSpec &spec : specs) {
      require(blockHashAt(spec, 1) == hash,
              "Four-validator devnet finalized divergent block hashes.");
    }
    nodes.stopAll();
    for (const NodeSpec &spec : specs) requireChainAuditPasses(spec, genesis);
    std::filesystem::remove_all(root, error);
  } catch (...) {
    nodes.stopAll();
    std::filesystem::remove_all(root, error);
    throw;
  }
}
#endif

} // namespace

int main() {
#ifdef _WIN32
  return 0;
#else
  try {
    testFourValidatorDevnet();
    std::cout << "Four-validator TCP devnet passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Four-validator TCP devnet failed: " << error.what() << '\n';
    return 1;
  }
#endif
}
