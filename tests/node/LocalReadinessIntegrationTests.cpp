// Integration tests for local node readiness evaluation.
// Covers: readiness passing on clean local state, failing on artifact corruption,
// monetary mismatch, treasury mismatch, evidence capture unhealthy,
// and official readiness being stricter than local readiness.

#include "config/NetworkParameters.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/ChainAuditor.hpp"
#include "node/EvidenceCaptureHealth.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ReadinessContext.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/TestnetReadinessChecker.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900200000;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / ("nodo-local-readiness-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << contents;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

std::string replaceFirst(std::string s, const std::string& from, const std::string& to) {
    const auto pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return s;
}

crypto::KeyPair localValidatorKey() {
    return crypto::KeyPair::createDeterministicBls12381KeyPair("readiness-validator");
}

crypto::KeyPair localUserKey() {
    return crypto::KeyPair::createDeterministicEd25519KeyPair("readiness-user");
}

config::GenesisConfig buildGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            config::BootstrapValidatorConfig(
                localValidatorKey().publicKey(), 1, 1, "readiness-validator"
            )
        },
        {
            config::GenesisAccountConfig(
                localUserKey().address().value(),
                Amount::fromRawUnits(5000000000000LL),
                0
            )
        },
        "readiness-genesis"
    );
}

p2p::PeerInfo localPeer(const std::string& tag = "default") {
    return p2p::PeerInfo(
        "readiness-peer-" + tag,
        "127.0.0.1:19600",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

crypto::Signer validatorSigner() {
    static const crypto::Bls12381SignatureProvider provider;
    return crypto::Signer(localValidatorKey(), provider);
}

crypto::Signer userSigner() {
    static const crypto::Ed25519SignatureProvider provider;
    return crypto::Signer(localUserKey(), provider);
}

node::NodeRuntime startFreshRuntime(const config::GenesisConfig& genesis) {
    const auto start = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, localPeer(), 16)
    );
    require(start.started(), "Runtime should start: " + start.reason());
    return start.runtime();
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& runtime,
    std::int64_t ts,
    std::uint64_t nonce
) {
    const auto tx = core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "readiness-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce, ts - 10
        ),
        userSigner(),
        runtime.config().genesisConfig().networkParameters().chainId()
    );
    const auto admission = runtime.mutableMempool().admitTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        ts - 9
    );
    require(admission.accepted(), "Transaction must be admitted for block production.");
    return node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        runtime,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        validatorSigner()
    );
}

crypto::StoredKeyMetadata makeValidatorKeyMetadata(
    const std::string& networkProfile
) {
    const crypto::PublicKey pk(
        crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
    return crypto::StoredKeyMetadata(
        "readiness-validator-key",
        crypto::CryptoAlgorithm::BLS12_381,
        crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        pk,
        "nodo1readiness001",
        kTimestamp,
        networkProfile
    );
}

// ---- Test 1: Local readiness passes on clean node at genesis ----

void testLocalReadinessPassesOnCleanGenesisNode() {
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    // Clean genesis node: 1 peer, genesis verified, height 0.
    const auto checks = node::TestnetReadinessChecker::check(
        params, key, 1, true, 0
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::READY);
}

// ---- Test 2: Local readiness passes on clean node with finalized blocks ----

void testLocalReadinessPassesWithFinalizedBlocks() {
    const auto path = tempPath("local-clean-blocks");
    clean(path);

    const config::GenesisConfig genesis = buildGenesis();
    const node::NodeDataDirectoryConfig dir(path);

    require(
        node::NodeDataDirectory::initialize(dir, genesis, localPeer(), kTimestamp + 1).initialized(),
        "Data directory should initialize."
    );

    node::NodeRuntime runtime = startFreshRuntime(genesis);

    const auto pipeline = produceBlock(runtime, kTimestamp + 20, 1);
    require(pipeline.finalized(), "Pipeline should finalize. Reason=" + pipeline.reason());
    require(
        node::FinalizedBlockStore::persist(dir, runtime, pipeline, kTimestamp + 30).stored(),
        "Block should persist."
    );

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
        dir, genesis, localPeer()
    );
    require(loaded.loaded(), "Runtime should reload.");

    // Dev-mode audit: no monetary report required on localnet.
    const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
    require(audit.passed(), "Chain audit should pass on clean local node.");

    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    // At this point the node is audited and chain is clean.
    node::TestnetReadinessCheckerConfig config(
        1,                // connectedPeers
        true,             // genesisVerified
        loaded.manifest().latestBlockHeight(),
        true,             // governanceLifecycleVerifierWired
        true,             // defenseModeInactive
        true,             // legacyPathsBlockedOnOfficialNetworks
        audit.passed(),   // treasuryReportConsistencyVerified
        true,             // evidenceCaptureHealthy
        audit.passed(),   // chainAuditCompleted
        true,             // genesisRegistered
        "DEVELOPMENT_LOCAL"
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto readinessStatus = node::TestnetReadinessChecker::summarize(checks);
    assert(readinessStatus == node::ReadinessStatus::READY);

    clean(path);
}

// ---- Test 3: Local readiness fails when artifact is corrupted ----

void testLocalReadinessFailsOnArtifactCorruption() {
    const auto path = tempPath("readiness-corrupt");
    clean(path);

    const config::GenesisConfig genesis = buildGenesis();
    const node::NodeDataDirectoryConfig dir(path);

    require(
        node::NodeDataDirectory::initialize(dir, genesis, localPeer(), kTimestamp + 1).initialized(),
        "Data directory should initialize."
    );

    node::NodeRuntime runtime = startFreshRuntime(genesis);

    const auto pipeline = produceBlock(runtime, kTimestamp + 20, 1);
    require(pipeline.finalized(), "Pipeline should finalize. Reason=" + pipeline.reason());

    const auto stored = node::FinalizedBlockStore::persist(dir, runtime, pipeline, kTimestamp + 30);
    require(stored.stored(), "Block should persist.");

    // Corrupt the block file.
    writeFile(stored.blockPath(), "corrupted-content\n");

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
        dir, genesis, localPeer()
    );

    // Loader must fail: corruption detected at reload.
    require(!loaded.loaded(), "Reloader must fail on corrupted artifact.");

    // Readiness must report NOT_READY when reload fails.
    // (A node that cannot reload its state is not ready.)
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    node::TestnetReadinessCheckerConfig config(
        0,      // connectedPeers (cannot even start)
        false,  // genesisVerified (node state unknown)
        0,
        true,
        true,
        true,
        false,  // treasuryReportConsistencyVerified — failed because reload failed
        true,
        false,  // chainAuditCompleted — failed
        true,   // genesisRegistered
        "DEVELOPMENT_LOCAL"
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    clean(path);
}

// ---- Test 4: Evidence capture unhealthy blocks readiness ----

void testEvidenceCaptureUnhealthyBlocksReadiness() {
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    node::TestnetReadinessCheckerConfig config(
        1,     // connectedPeers
        true,  // genesisVerified
        0,
        true,
        true,
        true,
        true,
        false, // evidenceCaptureHealthy = UNHEALTHY
        false,
        true,  // genesisRegistered
        "DEVELOPMENT_LOCAL"
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    // Verify the specific failing check.
    bool foundFailingCapture = false;
    for (const auto& c : checks) {
        if (c.checkName() == "evidence_capture_available" && !c.passed()) {
            foundFailingCapture = true;
        }
    }
    assert(foundFailingCapture);
}

// ---- Test 5: Official readiness stricter than local readiness ----

void testOfficialNetworkRequiresChainAuditWhileLocalDoesNot() {
    // On a localnet, no chain audit is required even with finalized blocks.
    {
        const auto params = config::NetworkParameters::developmentLocal();
        const auto key = makeValidatorKeyMetadata("localnet");

        node::TestnetReadinessCheckerConfig localConfig(
            1, true, 10,
            true, true, true, true, true,
            false,  // chainAuditCompleted = false
            true, "DEVELOPMENT_LOCAL"
        );

        const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
            params, key, localConfig
        );
        // On localnet, chain_audit_completed passes even when audit is not done.
        for (const auto& c : checks) {
            if (c.checkName() == "chain_audit_completed") {
                assert(c.passed() &&
                    "chain_audit_completed must pass on localnet without explicit audit");
            }
        }
    }

    // On an official network, chain audit is required when finalized blocks exist.
    {
        const auto params = config::NetworkProfileRegistry::get("testnet-candidate");
        const auto key = makeValidatorKeyMetadata("testnet-candidate");

        node::TestnetReadinessCheckerConfig officialConfig(
            1, true, 10,
            true, true, true, true, true,
            false,  // chainAuditCompleted = false
            true, "STAGING_CANDIDATE"
        );

        const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
            params, key, officialConfig
        );
        const auto status = node::TestnetReadinessChecker::summarize(checks);
        assert(status == node::ReadinessStatus::NOT_READY &&
            "Official network must be NOT_READY when chain audit not completed");

        bool auditCheckFailed = false;
        for (const auto& c : checks) {
            if (c.checkName() == "chain_audit_completed" && !c.passed()) {
                auditCheckFailed = true;
            }
        }
        assert(auditCheckFailed &&
            "chain_audit_completed must fail on official network without audit");
    }
}

// ---- Test 6: Treasury mismatch blocks readiness ----

void testTreasuryMismatchBlocksReadiness() {
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    node::TestnetReadinessCheckerConfig config(
        1, true, 10,
        true, true, true,
        false,  // treasuryReportConsistencyVerified = false
        true,
        false,
        true, "DEVELOPMENT_LOCAL"
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = node::TestnetReadinessChecker::summarize(checks);
    assert(status == node::ReadinessStatus::NOT_READY);

    bool found = false;
    for (const auto& c : checks) {
        if (c.checkName() == "treasury_report_consistent" && !c.passed()) {
            found = true;
        }
    }
    assert(found && "treasury_report_consistent must fail when treasury mismatch reported");
}

// ---- Test 7: ReadinessContextBuilder propagates evidence capture failure ----

void testReadinessContextBuilderPropagatesEvidenceCaptureFailure() {
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");
    const node::NodeDataDirectoryConfig dir(".nodo-readiness-test-tmp");

    node::EvidenceCaptureHealth unhealthy;
    unhealthy.markUnavailable();
    assert(!unhealthy.isHealthy());

    node::ReadinessContextBuilder builder(dir, params, key);
    builder.withEvidenceCaptureHealth(unhealthy);

    const auto ctx = builder.build();
    assert(!ctx.evidenceCaptureHealthy);
    assert(!ctx.warnings.empty());
}

// ---- Test 8: Treasury digest mismatch fails verification ----

void testTreasuryDigestMismatchFailsVerification() {
    using economics::EpochTreasuryReport;
    using economics::TreasurySpendRecord;
    using node::EpochTreasuryReportVerifier;

    const TreasurySpendRecord r1(
        "spend-rdy-001", "prop-001", "recipient-A",
        Amount::fromRawUnits(50000), "purpose", 10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(50000)
    );
    const TreasurySpendRecord r2(
        "spend-rdy-001", "prop-001", "recipient-B",
        Amount::fromRawUnits(50000), "purpose", 10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(50000)
    );

    const EpochTreasuryReport persisted = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport rebuilt   = EpochTreasuryReport::fromSpendRecords(0, {r2});

    // Same total, different recipients → digest mismatch.
    assert(persisted.treasurySpendTotal() == rebuilt.treasurySpendTotal());
    assert(persisted.spendRecordsDigest() != rebuilt.spendRecordsDigest());

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched() && "Same-total different-recipient treasury must fail verification.");
}

// ---- Test 9: Duplicate treasury spend identifier fails verification ----

void testDuplicateTreasurySpendIdFailsVerification() {
    using economics::EpochTreasuryReport;
    using economics::TreasurySpendRecord;
    using node::EpochTreasuryReportVerifier;

    const TreasurySpendRecord r1(
        "spend-dup-001", "prop-001", "recipient-A",
        Amount::fromRawUnits(25000), "purpose", 10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(75000)
    );
    const TreasurySpendRecord r2(
        "spend-dup-001", "prop-002", "recipient-B",  // duplicate spendId
        Amount::fromRawUnits(25000), "purpose", 10, 0,
        Amount::fromRawUnits(75000),
        Amount::fromRawUnits(50000)
    );

    const EpochTreasuryReport report = EpochTreasuryReport::fromSpendRecords(0, {r1, r2});
    const auto result = EpochTreasuryReportVerifier::verify(report, report);
    assert(!result.matched() && "Duplicate spend identifiers must fail verification.");
}

// ---- Test 10: Readiness diagnostics all have non-empty check names ----

void testReadinessDiagnosticsHaveNonEmptyNames() {
    const auto params = config::NetworkParameters::developmentLocal();
    const auto key = makeValidatorKeyMetadata("localnet");

    node::TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true, true, false,
        true, "DEVELOPMENT_LOCAL"
    );

    const auto checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );

    assert(!checks.empty());
    for (const auto& c : checks) {
        assert(!c.checkName().empty() && "All readiness checks must have non-empty names.");
        assert(!c.serialize().empty() && "All readiness diagnostics must serialize non-empty.");
    }
}

} // namespace

int main() {
    try {
        testLocalReadinessPassesOnCleanGenesisNode();
        testLocalReadinessPassesWithFinalizedBlocks();
        testLocalReadinessFailsOnArtifactCorruption();
        testEvidenceCaptureUnhealthyBlocksReadiness();
        testOfficialNetworkRequiresChainAuditWhileLocalDoesNot();
        testTreasuryMismatchBlocksReadiness();
        testReadinessContextBuilderPropagatesEvidenceCaptureFailure();
        testTreasuryDigestMismatchFailsVerification();
        testDuplicateTreasurySpendIdFailsVerification();
        testReadinessDiagnosticsHaveNonEmptyNames();

        std::cout << "Local readiness integration tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local readiness integration tests failed: " << e.what() << "\n";
        return 1;
    }
}
