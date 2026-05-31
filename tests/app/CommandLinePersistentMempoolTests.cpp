#include "app/CommandLineInterface.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

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

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-cli-persistent-mempool-tests-" + suffix);
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

void testSubmitThenProduceFromPersistentMempool() {
    const std::filesystem::path path =
        tempPath("submit-produce");

    clean(path);

    const auto init =
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-persistent-mempool-peer",
                "--endpoint",
                "127.0.0.1:9800",
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        init.success(),
        "Init should succeed."
    );

    const auto key =
        CommandLineInterface::execute(
            {
                "keys",
                "create",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 50)
            }
        );

    requireCondition(
        key.success(),
        "Key creation should succeed."
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
        "tx submit should persist pending transaction."
    );

    const auto produce =
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-persistent-mempool-peer",
                "--endpoint",
                "127.0.0.1:9800",
                "--timestamp",
                std::to_string(kTimestamp + 200)
            }
        );

    requireCondition(
        produce.success() &&
        produce.message().find("Block height: 1") != std::string::npos,
        "Produce should finalize block from persistent mempool."
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
        "Status should show latest height after persisted block."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testSubmitThenProduceFromPersistentMempool();

        std::cout << "Nodo command line persistent mempool tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo command line persistent mempool tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
