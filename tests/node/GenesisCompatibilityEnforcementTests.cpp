// Tests for genesis compatibility enforcement in runtime startup and data directory loading.
// Covers: data directory from wrong genesis rejected, correct genesis accepted,
// genesis ID mismatch in manifest, missing genesis ID in manifest, missing registry genesis.

#include "config/GenesisRegistry.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PublicKey.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeStartupService.hpp"
#include "node/TestnetReadinessChecker.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900300000;

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path() / ("nodo-genesis-compat-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

p2p::PeerInfo localPeer() {
    return p2p::PeerInfo("genesis-compat-peer", "127.0.0.1:19700", "nodo/0.1", 0, kTs);
}

// ---- Test 1: Correct genesis accepted ----

void testCorrectGenesisAccepted() {
    const auto path = tempPath("correct-genesis");
    clean(path);

    const config::GenesisLookupResult lookup =
        config::GenesisRegistry::get("localnet");
    assert(lookup.found());

    const node::NodeDataDirectoryConfig dir(path);
    const auto init = node::NodeDataDirectory::initialize(
        dir, lookup.genesis(), localPeer(), kTs
    );
    assert(init.success());

    const auto manifest = node::NodeDataDirectory::loadManifest(dir);
    assert(manifest.loaded());

    const node::StartupValidationResult result =
        node::RuntimeStartupService::validateDataDirectoryCompatibility(
            manifest.manifest(), lookup.genesis()
        );
    assert(result.valid());

    clean(path);
}

// ---- Test 2: Data directory from wrong genesis is rejected ----

void testWrongGenesisRejected() {
    const auto path = tempPath("wrong-genesis");
    clean(path);

    // Build a genesis different from the registry
    const config::GenesisConfig wrongGenesis(
        config::NetworkParameters::developmentLocal(),
        kTs,
        {
            config::BootstrapValidatorConfig(
                crypto::KeyPair::createDeterministicBls12381KeyPair("wrong-validator-seed").publicKey(),
                1, 1, "wrong-validator"
            )
        },
        {
            config::GenesisAccountConfig(
                crypto::KeyPair::createDeterministicEd25519KeyPair("wrong-user-seed").address().value(),
                utils::Amount::fromRawUnits(1000000000000LL),
                0
            )
        },
        "wrong-genesis-label"
    );

    const node::NodeDataDirectoryConfig dir(path);
    const auto init = node::NodeDataDirectory::initialize(
        dir, wrongGenesis, localPeer(), kTs
    );
    assert(init.success());

    const auto manifest = node::NodeDataDirectory::loadManifest(dir);
    assert(manifest.loaded());

    const config::GenesisLookupResult lookup =
        config::GenesisRegistry::get("localnet");
    assert(lookup.found());

    // Validate compatibility with the registry genesis — must fail.
    const node::StartupValidationResult result =
        node::RuntimeStartupService::validateDataDirectoryCompatibility(
            manifest.manifest(), lookup.genesis()
        );
    assert(!result.valid());
    assert(result.reason().find("genesis") != std::string::npos ||
           result.reason().find("Genesis") != std::string::npos);

    clean(path);
}

// ---- Test 3: Network name mismatch is rejected ----

void testNetworkMismatchRejected() {
    const auto path = tempPath("net-mismatch");
    clean(path);

    const config::GenesisLookupResult localnetLookup =
        config::GenesisRegistry::get("localnet");
    assert(localnetLookup.found());

    const node::NodeDataDirectoryConfig dir(path);
    const auto init = node::NodeDataDirectory::initialize(
        dir, localnetLookup.genesis(), localPeer(), kTs
    );
    assert(init.success());

    const auto manifest = node::NodeDataDirectory::loadManifest(dir);
    assert(manifest.loaded());

    const config::GenesisLookupResult testnetLookup =
        config::GenesisRegistry::get("testnet-candidate");
    assert(testnetLookup.found());

    // Validate compatibility with testnet-candidate genesis — must fail (network mismatch).
    const node::StartupValidationResult result =
        node::RuntimeStartupService::validateDataDirectoryCompatibility(
            manifest.manifest(), testnetLookup.genesis()
        );
    assert(!result.valid());

    clean(path);
}

// ---- Test 4: resolveAndVerify returns valid genesis for localnet ----

void testResolveAndVerifyLocalnetValid() {
    const config::GenesisLookupResult result =
        node::RuntimeStartupService::resolveAndVerify("localnet");
    assert(result.found());
    assert(!result.genesis().deterministicId().empty());
}

// ---- Test 5: resolveAndVerify rejects unknown network ----

void testResolveAndVerifyUnknownRejected() {
    const config::GenesisLookupResult result =
        node::RuntimeStartupService::resolveAndVerify("nonexistent-network");
    assert(!result.found());
    assert(!result.reason().empty());
}

// ---- Test 6: Readiness reports genesis mismatch when manifest differs ----

void testReadinessReportsGenesisMismatch() {
    const config::NetworkParameters params =
        config::NetworkParameters::developmentLocal();
    const crypto::StoredKeyMetadata key(
        "test-key",
        crypto::CryptoAlgorithm::BLS12_381,
        crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, std::string(96, 'b')),
        "nodo1test001",
        kTs,
        "localnet"
    );

    node::TestnetReadinessCheckerConfig config(
        1,     // connectedPeers
        true,  // genesisVerified
        0,     // finalizedHeight
        true,  // governanceLifecycleVerifierWired
        true,  // defenseModeInactive
        true,  // legacyPathsBlockedOnOfficialNetworks
        true,  // treasuryReportConsistencyVerified
        true,  // evidenceCaptureHealthy
        false, // chainAuditCompleted
        true,  // genesisRegistered
        "DEVELOPMENT_LOCAL",
        "registered-genesis-abc",  // registeredGenesisId
        "different-manifest-xyz",  // manifestGenesisId — intentional mismatch
        false, // finalityVerificationActive
        true,  // peerCompatibilityPassed
        true,  // latestImportSucceeded
        "",    // latestImportRejectionReason
        false  // defenseRestrictionsActive
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    bool foundGenesisMismatch = false;
    for (const auto& c : checks) {
        if (c.checkName() == "genesis_compatibility" && !c.passed()) {
            foundGenesisMismatch = true;
            assert(c.detail().find("registered-genesis-abc") != std::string::npos);
            assert(c.detail().find("different-manifest-xyz") != std::string::npos);
        }
    }
    assert(foundGenesisMismatch && "genesis_compatibility must fail on mismatch");
}

// ---- Test 7: Readiness passes when genesis IDs match ----

void testReadinessPassesWhenGenesisCompatible() {
    const config::NetworkParameters params =
        config::NetworkParameters::developmentLocal();
    const crypto::StoredKeyMetadata key(
        "test-key",
        crypto::CryptoAlgorithm::BLS12_381,
        crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, std::string(96, 'b')),
        "nodo1test001",
        kTs,
        "localnet"
    );

    const std::string sameGenesisId = "matching-genesis-abc";

    node::TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true, true, false, true,
        "DEVELOPMENT_LOCAL",
        sameGenesisId,  // registeredGenesisId
        sameGenesisId,  // manifestGenesisId — match
        false, true, true, "", false
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );

    for (const auto& c : checks) {
        if (c.checkName() == "genesis_compatibility") {
            assert(c.passed() && "genesis_compatibility must pass when IDs match");
        }
    }
}

// ---- Test 8: Readiness reports import failure ----

void testReadinessReportsImportFailure() {
    const config::NetworkParameters params =
        config::NetworkParameters::developmentLocal();
    const crypto::StoredKeyMetadata key(
        "test-key",
        crypto::CryptoAlgorithm::BLS12_381,
        crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, std::string(96, 'b')),
        "nodo1test001",
        kTs,
        "localnet"
    );

    node::TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true, true, false, true,
        "DEVELOPMENT_LOCAL", "", "", false, true,
        false,                          // latestImportSucceeded = false
        "HEIGHT_CONTINUITY_MISMATCH",   // latestImportRejectionReason
        false
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    bool foundImportFail = false;
    for (const auto& c : checks) {
        if (c.checkName() == "latest_import_succeeded" && !c.passed()) {
            foundImportFail = true;
            assert(c.detail().find("HEIGHT_CONTINUITY_MISMATCH") != std::string::npos);
        }
    }
    assert(foundImportFail && "latest_import_succeeded must fail when import failed");
}

// ---- Test 9: Defense restrictions block readiness ----

void testDefenseRestrictionsBlockReadiness() {
    const config::NetworkParameters params =
        config::NetworkParameters::developmentLocal();
    const crypto::StoredKeyMetadata key(
        "test-key",
        crypto::CryptoAlgorithm::BLS12_381,
        crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, std::string(96, 'b')),
        "nodo1test001",
        kTs,
        "localnet"
    );

    node::TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true, true, false, true,
        "DEVELOPMENT_LOCAL", "", "", false, true, true, "",
        true  // defenseRestrictionsActive = true
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    bool found = false;
    for (const auto& c : checks) {
        if (c.checkName() == "defense_mode_economic_restrictions" && !c.passed()) {
            found = true;
        }
    }
    assert(found && "defense_mode_economic_restrictions must fail when restrictions active");
}

} // namespace

int main() {
    try {
        testCorrectGenesisAccepted();
        testWrongGenesisRejected();
        testNetworkMismatchRejected();
        testResolveAndVerifyLocalnetValid();
        testResolveAndVerifyUnknownRejected();
        testReadinessReportsGenesisMismatch();
        testReadinessPassesWhenGenesisCompatible();
        testReadinessReportsImportFailure();
        testDefenseRestrictionsBlockReadiness();

        std::cout << "Genesis compatibility enforcement tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Genesis compatibility enforcement tests failed: " << e.what() << "\n";
        return 1;
    }
}
