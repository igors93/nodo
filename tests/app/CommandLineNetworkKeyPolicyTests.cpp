// Regression coverage for the unified key/crypto policy across
// crypto::ProtocolCryptoContext, node::ProductionKeySafetyGate, and every
// CLI command that signs on official networks. Before this policy was
// aligned, ProtocolCryptoContext::testnet() was permanently invalid by
// construction, which unconditionally blocked tx submit, governance
// propose/vote/execute, validator exit/unjail, and stake lock family on
// testnet-candidate regardless of key quality, while executeKeysCreate
// blanket-blocked key creation for every official network (making
// testnet-candidate unreachable in practice). These tests prove the fix:
// testnet-candidate is now a first-class, usable profile gated only by key
// custody (ProductionKeySafetyGate/KeyEncryptionPolicy), never by a
// permanently-broken crypto context.

#include "app/CommandLineInterface.hpp"

#include "config/GenesisRegistry.hpp"
#include "crypto/KeyStore.hpp"

#include <cstdlib>
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

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-network-key-policy-tests-" + suffix);
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

void setTestEnvVar(
    const char* name,
    const char* value
) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void unsetTestEnvVar(
    const char* name
) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

void initTestnetCandidate(
    const std::filesystem::path& path
) {
    requireCondition(
        CommandLineInterface::execute({
            "init",
            "--network", "testnet-candidate",
            "--data-dir", path.string(),
            "--timestamp", std::to_string(kTimestamp)
        }).success(),
        "testnet-candidate init should succeed."
    );
}

// Places a localnet-profile plaintext key directly into a data directory's
// keys folder, bypassing the CLI, so tests can exercise
// ProductionKeySafetyGate deterministically without depending on interactive
// password prompts (there is no way to create such a key through the CLI on
// an official network any more, which is the point of this fix).
void seedLocalnetOnlyKey(
    const std::filesystem::path& dataDir,
    const std::string& keyId,
    nodo::crypto::KeyStoreKeyType keyType,
    const std::string& seed
) {
    const auto result = nodo::crypto::KeyStore::createLocalKey(
        dataDir / "keys", keyId, keyType, seed, kTimestamp
    );
    requireCondition(
        result.success(),
        "Test setup: localnet-only key creation should succeed."
    );
}

std::string testnetCandidateBootstrapValidatorAddress() {
    const auto genesis = nodo::config::GenesisRegistry::get("testnet-candidate");
    requireCondition(genesis.found(), "testnet-candidate genesis must exist.");
    return genesis.genesis().bootstrapValidators().front().validatorAddress();
}

// A rejection that still names "crypto context" means the permanently
// invalid ProtocolCryptoContext::testnet() bug is back — this must never
// happen again regardless of which key was used.
void requireNoCryptoContextRejection(
    const std::string& message,
    const std::string& commandLabel
) {
    requireCondition(
        message.find("crypto context") == std::string::npos,
        commandLabel +
            " must not fail with a crypto-context validity error on "
            "testnet-candidate: " + message
    );
}

void requireKeySafetyRejection(
    const std::string& message,
    const std::string& commandLabel
) {
    requireCondition(
        message.find("localnet-only") != std::string::npos ||
            message.find("official network") != std::string::npos,
        commandLabel +
            " should fail with a key-safety rejection reason instead: " +
            message
    );
}

void testKeysCreateSucceedsOnTestnetCandidateWithPassword() {
    const std::filesystem::path path = tempPath("keys-create");
    clean(path);
    initTestnetCandidate(path);

    setTestEnvVar("NODO_KEY_PASSWORD", "network-key-policy-tests-password");
    const auto created = CommandLineInterface::execute({
        "keys", "create",
        "--network", "testnet-candidate",
        "--data-dir", path.string(),
        "--type", "both",
        "--timestamp", std::to_string(kTimestamp + 1)
    });
    unsetTestEnvVar("NODO_KEY_PASSWORD");

    requireCondition(
        created.success() &&
            created.message().find("Network profile: testnet-candidate") !=
                std::string::npos,
        "keys create should now succeed for testnet-candidate given an "
        "encryption password: " + created.message()
    );

    clean(path);
}

void testTxSubmitNoLongerBlockedByCryptoContext() {
    const std::filesystem::path path = tempPath("tx-submit");
    clean(path);
    initTestnetCandidate(path);

    seedLocalnetOnlyKey(
        path, "local-user", nodo::crypto::KeyStoreKeyType::USER,
        "tx-submit-localnet-only-user-seed"
    );

    const auto result = CommandLineInterface::execute({
        "tx", "submit",
        "--network", "testnet-candidate",
        "--data-dir", path.string(),
        "--timestamp", std::to_string(kTimestamp + 10)
    });

    requireCondition(
        !result.success(),
        "A localnet-only key must still be rejected on testnet-candidate."
    );
    requireNoCryptoContextRejection(result.message(), "tx submit");
    requireKeySafetyRejection(result.message(), "tx submit");

    clean(path);
}

void testGovernanceProposeNoLongerBlockedByCryptoContext() {
    const std::filesystem::path path = tempPath("governance-propose");
    clean(path);
    initTestnetCandidate(path);

    seedLocalnetOnlyKey(
        path, "local-user", nodo::crypto::KeyStoreKeyType::USER,
        "governance-propose-localnet-only-user-seed"
    );

    const auto result = CommandLineInterface::execute({
        "governance", "propose",
        "--network", "testnet-candidate",
        "--data-dir", path.string(),
        "--proposal-type", "text",
        "--title", "Test proposal",
        "--body", "Test body",
        "--timestamp", std::to_string(kTimestamp + 10)
    });

    requireCondition(
        !result.success(),
        "A localnet-only key must still be rejected on testnet-candidate."
    );
    requireNoCryptoContextRejection(result.message(), "governance propose");
    requireKeySafetyRejection(result.message(), "governance propose");

    clean(path);
}

void testValidatorExitNoLongerBlockedByCryptoContext() {
    const std::filesystem::path path = tempPath("validator-exit");
    clean(path);
    initTestnetCandidate(path);

    seedLocalnetOnlyKey(
        path, "local-validator", nodo::crypto::KeyStoreKeyType::VALIDATOR,
        "validator-exit-localnet-only-validator-seed"
    );

    const auto result = CommandLineInterface::execute({
        "validator", "exit",
        "--network", "testnet-candidate",
        "--data-dir", path.string(),
        "--validator", testnetCandidateBootstrapValidatorAddress(),
        "--timestamp", std::to_string(kTimestamp + 10)
    });

    requireCondition(
        !result.success(),
        "A localnet-only key must still be rejected on testnet-candidate."
    );
    requireNoCryptoContextRejection(result.message(), "validator exit");
    requireKeySafetyRejection(result.message(), "validator exit");

    clean(path);
}

void testStakeLockNoLongerBlockedByCryptoContext() {
    const std::filesystem::path path = tempPath("stake-lock");
    clean(path);
    initTestnetCandidate(path);

    seedLocalnetOnlyKey(
        path, "local-user", nodo::crypto::KeyStoreKeyType::USER,
        "stake-lock-localnet-only-user-seed"
    );

    const auto result = CommandLineInterface::execute({
        "stake", "lock",
        "--network", "testnet-candidate",
        "--data-dir", path.string(),
        "--validator", testnetCandidateBootstrapValidatorAddress(),
        "--amount", "1000",
        "--timestamp", std::to_string(kTimestamp + 10)
    });

    requireCondition(
        !result.success(),
        "A localnet-only key must still be rejected on testnet-candidate."
    );
    requireNoCryptoContextRejection(result.message(), "stake lock");
    requireKeySafetyRejection(result.message(), "stake lock");

    clean(path);
}

void testNodeRunNoLongerBlockedByCryptoContext() {
    const std::filesystem::path path = tempPath("node-run");
    clean(path);
    initTestnetCandidate(path);

    // No keys at all: node run should now fail for the identity-key reason,
    // not because executeNodeRun bypassed the crypto-context gate entirely
    // (its old bug) or because the gate itself was permanently invalid.
    const auto result = CommandLineInterface::execute({
        "node", "run",
        "--network", "testnet-candidate",
        "--data-dir", path.string()
    });

    requireCondition(
        !result.success(),
        "node run with no identity key should fail."
    );
    requireNoCryptoContextRejection(result.message(), "node run");
    requireCondition(
        result.message().find("Ed25519 local-user key is required") !=
            std::string::npos,
        "node run should fail on the missing identity key, not an earlier "
        "crypto-context error: " + result.message()
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testKeysCreateSucceedsOnTestnetCandidateWithPassword();
        testTxSubmitNoLongerBlockedByCryptoContext();
        testGovernanceProposeNoLongerBlockedByCryptoContext();
        testValidatorExitNoLongerBlockedByCryptoContext();
        testStakeLockNoLongerBlockedByCryptoContext();
        testNodeRunNoLongerBlockedByCryptoContext();

        std::cout << "Nodo network key policy tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo network key policy tests failed: "
                  << error.what() << "\n";
        return 1;
    }
}
