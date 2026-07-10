#include "app/CommandLineInterface.hpp"

#include "crypto/KeyStore.hpp"

#include <cstdlib>
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
        result.message().find("nodo tx submit") != std::string::npos &&
        result.message().find("nodo chain audit") != std::string::npos &&
        result.message().find("nodo governance propose") != std::string::npos &&
        result.message().find("nodo governance vote") != std::string::npos &&
        result.message().find("--network") != std::string::npos &&
        result.message().find("nodo testnet readiness") != std::string::npos &&
        result.message().find("nodo diagnostics") != std::string::npos,
        "Help command should succeed and include protocol command usage."
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

void testNetworkProfileSelectionAndMainnetLock() {
    const std::filesystem::path localPath =
        tempPath("network-localnet");

    const std::filesystem::path testnetPath =
        tempPath("network-testnet");

    clean(localPath);
    clean(testnetPath);

    const auto localInit =
        CommandLineInterface::execute(
            {
                "init",
                "--network",
                "localnet",
                "--data-dir",
                localPath.string(),
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        localInit.success() &&
        localInit.message().find("Network: localnet") != std::string::npos,
        "Localnet network profile should initialize explicitly."
    );

    const auto localStatus =
        CommandLineInterface::execute(
            {
                "status",
                "--network",
                "localnet",
                "--data-dir",
                localPath.string()
            }
        );

    requireCondition(
        localStatus.success(),
        "Status should accept matching localnet profile."
    );

    const auto testnetInit =
        CommandLineInterface::execute(
            {
                "init",
                "--network",
                "testnet-candidate",
                "--data-dir",
                testnetPath.string(),
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        testnetInit.success() &&
        testnetInit.message().find("Network: testnet-candidate") != std::string::npos,
        "Testnet-candidate network profile should initialize with its canonical profile."
    );

    const auto mismatchStatus =
        CommandLineInterface::execute(
            {
                "status",
                "--network",
                "localnet",
                "--data-dir",
                testnetPath.string()
            }
        );

    requireCondition(
        !mismatchStatus.success() &&
        mismatchStatus.message().find("selected network") != std::string::npos,
        "CLI should reject data directory and selected network mismatch."
    );

    const auto unknown =
        CommandLineInterface::execute(
            {
                "status",
                "--network",
                "unknown-network",
                "--data-dir",
                localPath.string()
            }
        );

    requireCondition(
        !unknown.success() &&
        unknown.status() == CommandLineStatus::INVALID_ARGUMENTS &&
        unknown.message().find("Unknown network profile") != std::string::npos,
        "Unknown network profile should fail clearly."
    );

    const auto mainnet =
        CommandLineInterface::execute(
            {
                "init",
                "--network",
                "mainnet",
                "--data-dir",
                tempPath("mainnet-lock").string(),
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        !mainnet.success() &&
        mainnet.message().find("mainnet") != std::string::npos &&
        mainnet.message().find("blocked") != std::string::npos,
        "Mainnet profile should remain explicitly blocked."
    );

    clean(localPath);
    clean(testnetPath);
    clean(tempPath("mainnet-lock"));
}

void testOfficialNetworkKeySafetyAndDiagnostics() {
    const std::filesystem::path localPath =
        tempPath("key-safety-localnet");

    const std::filesystem::path testnetPath =
        tempPath("key-safety-testnet");

    clean(localPath);
    clean(testnetPath);

    requireCondition(
        CommandLineInterface::execute(
            {
                "init",
                "--network",
                "localnet",
                "--data-dir",
                localPath.string(),
                "--timestamp",
                std::to_string(kTimestamp)
            }
        ).success(),
        "Localnet init should succeed before key safety test."
    );

    requireCondition(
        CommandLineInterface::execute(
            {
                "keys",
                "create",
                "--network",
                "localnet",
                "--data-dir",
                localPath.string(),
                "--timestamp",
                std::to_string(kTimestamp + 1)
            }
        ).success(),
        "Localnet should allow local plaintext development keys."
    );

    const auto localReadiness =
        CommandLineInterface::execute(
            {
                "testnet",
                "readiness",
                "--network",
                "localnet",
                "--data-dir",
                localPath.string(),
                "--key-id",
                "local-validator"
            }
        );

    requireCondition(
        localReadiness.success() &&
        localReadiness.message().find("Key policy passed: yes") != std::string::npos,
        "Localnet readiness path should accept a local key policy."
    );

    requireCondition(
        CommandLineInterface::execute(
            {
                "init",
                "--network",
                "testnet-candidate",
                "--data-dir",
                testnetPath.string(),
                "--timestamp",
                std::to_string(kTimestamp)
            }
        ).success(),
        "Testnet-candidate init should succeed before key safety test."
    );

    // testnet-candidate key creation is only unblocked when the resulting
    // key is encrypted (TESTNET_SAFE); supply the password non-interactively
    // via NODO_KEY_PASSWORD so this test does not block on a TTY prompt.
    setenv("NODO_KEY_PASSWORD", "cli-tests-official-key-password", 1);
    const auto createOfficialKey =
        CommandLineInterface::execute(
            {
                "keys",
                "create",
                "--network",
                "testnet-candidate",
                "--data-dir",
                testnetPath.string(),
                "--type",
                "validator",
                "--key-id",
                "official-validator-key",
                "--timestamp",
                std::to_string(kTimestamp + 2)
            }
        );
    unsetenv("NODO_KEY_PASSWORD");

    requireCondition(
        createOfficialKey.success() &&
        createOfficialKey.message().find(
            "Network profile: testnet-candidate") != std::string::npos,
        "Encrypted key creation should now be allowed for testnet-candidate."
    );

    const auto bypassCreated =
        nodo::crypto::KeyStore::createLocalKey(
            testnetPath / "keys",
            "local-validator",
            nodo::crypto::KeyStoreKeyType::VALIDATOR,
            "cli-testnet-localnet-only-key",
            kTimestamp + 3
        );

    requireCondition(
        bypassCreated.success(),
        "Test setup should create a localnet-only key directly for safety gate coverage."
    );

    const auto readiness =
        CommandLineInterface::execute(
            {
                "testnet",
                "readiness",
                "--network",
                "testnet-candidate",
                "--data-dir",
                testnetPath.string(),
                "--key-id",
                "local-validator"
            }
        );

    requireCondition(
        readiness.success() &&
        readiness.message().find("Genesis verified: yes") != std::string::npos &&
        readiness.message().find("Key policy passed: no") != std::string::npos &&
        readiness.message().find("localnet-only plaintext key") != std::string::npos &&
        readiness.message().find("Readiness: NOT_READY") != std::string::npos,
        "Testnet-candidate readiness should reject insecure localnet-only keys with diagnostics."
    );

    const auto diagnostics =
        CommandLineInterface::execute(
            {
                "diagnostics",
                "--network",
                "testnet-candidate",
                "--data-dir",
                testnetPath.string(),
                "--key-id",
                "local-validator"
            }
        );

    requireCondition(
        diagnostics.success() &&
        diagnostics.message().find("network=testnet-candidate") != std::string::npos &&
        diagnostics.message().find("genesisVerified=yes") != std::string::npos &&
        diagnostics.message().find("keyPolicyPassed=no") != std::string::npos &&
        diagnostics.message().find("readiness=NOT_READY") != std::string::npos,
        "Diagnostics should expose NOT_READY reasons for testnet-candidate."
    );

    clean(localPath);
    clean(testnetPath);
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
        testNetworkProfileSelectionAndMainnetLock();
        testOfficialNetworkKeySafetyAndDiagnostics();
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
