#include "app/CommandLineInterface.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path()
        / "nodo-cli-local-flow-tests";
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(
        path,
        error
    );
}

void testLocalRuntimeFlow() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const std::vector<std::string> peerOptions = {
        "--peer-id",
        "cli-local-flow-peer",
        "--endpoint",
        "127.0.0.1:9900"
    };

    const auto init =
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        init.success(),
        "Init should succeed."
    );

    const auto submit =
        CommandLineInterface::execute(
            {
                "tx",
                "submit",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 100)
            }
        );

    requireCondition(
        submit.success() &&
        submit.message().find("Transaction id:") != std::string::npos,
        "Demo transaction should be persisted before block production."
    );

    const auto produce =
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp + 200)
            }
        );

    requireCondition(
        produce.success() &&
        produce.message().find("Block height: 1") != std::string::npos &&
        produce.message().find("Pending transactions removed: 1") != std::string::npos,
        "Demo block should finalize the submitted transaction."
    );

    const auto reload =
        CommandLineInterface::execute(
            {
                "node",
                "reload",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp + 300)
            }
        );

    requireCondition(
        reload.success() &&
        reload.message().find("Loaded finalized blocks: 1") != std::string::npos &&
        reload.message().find("Loaded mempool transactions: 0") != std::string::npos,
        "Reload should rebuild the runtime from finalized block and empty mempool."
    );

    const auto audit =
        CommandLineInterface::execute(
            {
                "chain",
                "audit",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp + 350)
            }
        );

    requireCondition(
        audit.success() &&
        audit.message().find("Nodo chain audit passed.") != std::string::npos &&
        audit.message().find("Loaded finalized blocks: 1") != std::string::npos,
        "Chain audit should verify the persisted local chain."
    );

    const auto status =
        CommandLineInterface::execute(
            {
                "status",
                "--data-dir",
                path.string()
            }
        );

    requireCondition(
        status.success() &&
        status.message().find("Latest height: 1") != std::string::npos,
        "Status should report the finalized height."
    );

    const auto inspect =
        CommandLineInterface::execute(
            {
                "inspect",
                "--data-dir",
                path.string()
            }
        );

    requireCondition(
        inspect.success() &&
        inspect.message().find("NodeRuntimeManifest{") != std::string::npos &&
        inspect.message().find("latestBlockHeight=1") != std::string::npos,
        "Inspect should print the updated manifest."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testLocalRuntimeFlow();

        std::cout << "Nodo command line local flow tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo command line local flow tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
