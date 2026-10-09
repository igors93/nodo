#include "../common/HistoryChainFixture.hpp"
#include "../common/TestFramework.hpp"

#include "node/history/HistoryStore.hpp"

#include <iostream>
#include <string>

using nodo::test::HistoryChainFixture;
using nodo::test::require;

namespace {

std::string run(const HistoryChainFixture &chain,
                std::vector<std::string> args, bool expectSuccess = true) {
  args.push_back("--data-dir");
  args.push_back(chain.path().string());
  const nodo::app::CommandLineResult result = chain.cli(args);
  require(result.success() == expectSuccess,
          args.front() + " " + args[1] + " unexpected result: " +
              result.message());
  return result.message();
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

int main() {
  try {
    HistoryChainFixture chain("cli-history");
    chain.produce(16);
    const nodo::node::HistoryStore store(chain.directory());
    const std::string id8 = store.loadCheckpoint(8)->checkpointId();

    require(contains(run(chain, {"checkpoint", "status"}), "Checkpoints: 2"),
            "checkpoint status counts checkpoints");
    const std::string list = run(chain, {"checkpoint", "list"});
    require(contains(list, "8 " + id8) && contains(list, "16 "),
            "checkpoint list shows every checkpoint id");
    require(contains(run(chain, {"checkpoint", "show", "--height", "8"}),
                     "\"checkpointId\":\"" + id8 + "\""),
            "checkpoint show prints the checkpoint");
    const std::string verify = run(chain, {"checkpoint", "verify"});
    require(contains(verify, "Checkpoint 8: ACCEPTED") &&
                contains(verify, "Checkpoint 16: ACCEPTED"),
            "checkpoint verify re-verifies QCs and snapshots");

    require(contains(run(chain, {"storage", "status"}), "Storage schema version: 2"),
            "storage status reports the schema");
    require(contains(run(chain, {"storage", "status", "--json"}),
                     "\"historyLayout\":true"),
            "storage status has a JSON form");
    require(contains(run(chain, {"storage", "migrate"}), "already current"),
            "migration is idempotent on a current directory");

    const std::string pruningStatus =
        run(chain, {"pruning", "status", "--mode", "normal"});
    require(contains(pruningStatus, "Block body pruning: BLOCKED") &&
                contains(pruningStatus, "3.17"),
            "pruning status explains why block bodies stay");
    require(contains(run(chain, {"pruning", "run", "--mode", "normal",
                                 "--dry-run"}),
                     "Pruning: DRY_RUN"),
            "dry runs touch nothing");
    const std::string pruned = run(chain, {"pruning", "run", "--mode", "normal"});
    require(contains(pruned, "Pruning: APPLIED") || contains(pruned, "Pruning: NOOP"),
            "pruning runs with the safe engine");

    require(contains(run(chain, {"archive", "status"}), "Committed segments: 4"),
            "archive status reports sealed segments");
    require(contains(run(chain, {"archive", "segments"}), "heights 13-16"),
            "archive segments lists commitments");
    require(contains(run(chain, {"archive", "self-audit"}), "Self-audit PASSED"),
            "a node can challenge its own copy of history");
    require(contains(run(chain, {"storage", "status", "--json"}),
                     "\"selfAudit\":{\"challenges\":1,\"passed\":1"),
            "self-audit results are recorded");
    require(contains(run(chain, {"archive", "enable"}), "Archive mode enabled"),
            "archive mode can be enabled");

    // A second node verifies the first one's data as an untrusted peer.
    HistoryChainFixture verifier("cli-history-verifier");
    const std::string now =
        std::to_string(HistoryChainFixture::timestampFor(16) + 60);
    const std::string verified = run(
        verifier, {"checkpoint", "verify-bootstrap", "--source-dir",
                   chain.path().string(), "--trusted-checkpoint",
                   "8:" + id8, "--timestamp", now});
    require(contains(verified, "Bootstrap verified") &&
                contains(verified, "Verified tip height: 16"),
            "bootstrap from a trusted checkpoint reaches the tip");
    require(contains(run(verifier,
                         {"checkpoint", "verify-bootstrap", "--source-dir",
                          chain.path().string(), "--trusted-checkpoint",
                          "8:" + std::string(64, 'f'), "--timestamp", now},
                         false),
                     "ANCHOR_MISMATCH"),
            "an unanchored checkpoint is rejected");

    // The legacy commands still work through the safe engine.
    run(chain, {"node", "pruning-status"});
    std::cout << "Command line history tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Command line history tests failed: " << error.what() << "\n";
    return 1;
  }
}
