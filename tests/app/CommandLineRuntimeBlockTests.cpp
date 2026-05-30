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
        / ("nodo-cli-runtime-block-tests-" + suffix);
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

void testProduceDemoBlockCommand() {
    const std::filesystem::path path =
        tempPath("produce-demo");

    clean(path);

    const auto init =
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-runtime-block-peer",
                "--endpoint",
                "127.0.0.1:9500",
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        init.success(),
        "Init should succeed before producing demo block."
    );

    const auto produce =
        CommandLineInterface::execute(
            {
                "produce-demo-block",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-runtime-block-peer",
                "--endpoint",
                "127.0.0.1:9500",
                "--timestamp",
                std::to_string(kTimestamp + 100)
            }
        );

    requireCondition(
        produce.success() &&
        produce.message().find("Block height: 1") != std::string::npos,
        "produce-demo-block should finalize and persist block height 1."
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
        "Status should show updated height after demo block."
    );

    clean(path);
}

void testProduceDemoBlockBeforeInitFails() {
    const std::filesystem::path path =
        tempPath("produce-before-init");

    clean(path);

    const auto produce =
        CommandLineInterface::execute(
            {
                "produce-demo-block",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 200)
            }
        );

    requireCondition(
        !produce.success(),
        "produce-demo-block should fail before init."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testProduceDemoBlockCommand();
        testProduceDemoBlockBeforeInitFails();

        std::cout << "Nodo command line runtime block tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo command line runtime block tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
