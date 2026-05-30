#include "app/CommandLineInterface.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineStatus;

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
        / ("nodo-cli-tests-" + suffix);
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

void testHelpCommand() {
    const auto result =
        CommandLineInterface::execute(
            {"help"}
        );

    requireCondition(
        result.success() &&
        result.message().find("nodo init") != std::string::npos,
        "Help command should succeed and include init usage."
    );
}

void testInitStatusInspectFlow() {
    const std::filesystem::path path =
        tempPath("flow");

    clean(path);

    const std::string dataDir =
        path.string();

    const auto init =
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                dataDir,
                "--peer-id",
                "cli-test-peer",
                "--endpoint",
                "127.0.0.1:9200",
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        init.success(),
        "CLI init should succeed."
    );

    const auto status =
        CommandLineInterface::execute(
            {
                "status",
                "--data-dir",
                dataDir
            }
        );

    requireCondition(
        status.success() &&
        status.message().find("Chain id:") != std::string::npos,
        "CLI status should succeed."
    );

    const auto inspect =
        CommandLineInterface::execute(
            {
                "inspect",
                "--data-dir",
                dataDir
            }
        );

    requireCondition(
        inspect.success() &&
        inspect.message().find("NodeRuntimeManifest") != std::string::npos,
        "CLI inspect should print manifest."
    );

    clean(path);
}

void testUnknownCommandFails() {
    const auto result =
        CommandLineInterface::execute(
            {"unknown-command"}
        );

    requireCondition(
        !result.success() &&
        result.status() == CommandLineStatus::INVALID_ARGUMENTS,
        "Unknown command should fail with INVALID_ARGUMENTS."
    );
}

void testStatusBeforeInitFails() {
    const std::filesystem::path path =
        tempPath("status-before-init");

    clean(path);

    const auto result =
        CommandLineInterface::execute(
            {
                "status",
                "--data-dir",
                path.string()
            }
        );

    requireCondition(
        !result.success(),
        "Status before init should fail."
    );
}

} // namespace

int main() {
    try {
        testHelpCommand();
        testInitStatusInspectFlow();
        testUnknownCommandFails();
        testStatusBeforeInitFails();

        std::cout << "Nodo command line interface tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo command line interface tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
