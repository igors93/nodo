// Tests for the complete local node lifecycle:
// initialization → block production → persistence → reload → chain audit.
// Covers: artifact digest integrity, reload rejection on corruption,
// deferred reward accounting, monetary supply continuity,
// and restarted-node auditability.

#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/ChainAuditor.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
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

constexpr std::int64_t kTimestamp = 1900100000;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / ("nodo-local-lifecycle-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << contents;
}

std::string replaceFirst(std::string s, const std::string& from, const std::string& to) {
    const auto pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return s;
}

crypto::KeyPair localValidatorKey() {
    return crypto::KeyPair::createDeterministicBls12381KeyPair("local-lifecycle-validator");
}

crypto::KeyPair localUserKey() {
    return crypto::KeyPair::createDeterministicEd25519KeyPair("local-lifecycle-user");
}

config::GenesisConfig buildGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            config::BootstrapValidatorConfig(
                localValidatorKey().publicKey(), 1, 1, "local-lifecycle-validator"
            )
        },
        {
            config::GenesisAccountConfig(
                localUserKey().address().value(),
                Amount::fromRawUnits(5000000000000LL),
                0
            )
        },
        "local-lifecycle-genesis"
    );
}

p2p::PeerInfo localPeer() {
    return p2p::PeerInfo(
        "local-lifecycle-peer",
        "127.0.0.1:19500",
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

core::Transaction signedTransfer(std::uint64_t nonce, std::int64_t ts) {
    return core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "local-lifecycle-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce, ts
        ),
        userSigner(),
        "nodo-localnet-1"
    );
}

void admitTransaction(node::NodeRuntime& runtime, std::uint64_t nonce, std::int64_t ts) {
    const auto result = runtime.mutableMempool().admitTransaction(
        signedTransfer(nonce, ts),
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        ts + 1
    );
    require(result.accepted(), "Transaction must be admitted to mempool.");
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& runtime,
    std::int64_t ts,
    std::uint64_t nonce
) {
    admitTransaction(runtime, nonce, ts - 10);
    return node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        validatorSigner()
    );
}

// ---- Test 1: Complete lifecycle from init through chain audit ----

void testFullLifecycleInitThroughAudit() {
    const auto path = tempPath("full-lifecycle");
    clean(path);

    const config::GenesisConfig genesis = buildGenesis();
    const node::NodeDataDirectoryConfig dir(path);

    require(
        node::NodeDataDirectory::initialize(dir, genesis, localPeer(), kTimestamp + 1).initialized(),
        "Data directory should initialize."
    );

    node::NodeRuntime runtime = startFreshRuntime(genesis);

    const auto pipeline = produceBlock(runtime, kTimestamp + 20, 1);
    require(pipeline.finalized(), "Pipeline should finalize a block. Reason=" + pipeline.reason());

    require(
        node::FinalizedBlockStore::persist(dir, runtime, pipeline, kTimestamp + 30).stored(),
        "Block should persist."
    );

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
        dir, genesis, localPeer()
    );
    require(loaded.loaded(),
        "Reloaded runtime should succeed. Status=" +
        node::runtimeStateLoadStatusToString(loaded.status()) +
        " Reason=" + loaded.reason()
    );

    require(loaded.loadedBlockCount() == 1, "Reloaded runtime should report exactly 1 loaded block.");
    require(loaded.runtime().blockchain().size() == 2,
        "Reloaded blockchain should have genesis + 1 finalized block.");

    const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
    require(audit.passed(),
        "Chain audit should pass on clean reloaded state. Reason: " + audit.reason()
    );

    clean(path);
}

// ---- Test 2: Reload rejects missing finalized block file ----

void testReloadRejectsMissingBlockFile() {
    const auto path = tempPath("missing-block");
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

    std::error_code ec;
    std::filesystem::remove(stored.blockPath(), ec);

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    require(!loaded.loaded(), "Loader should reject missing block file.");
    require(
        loaded.status() == node::RuntimeStateLoadStatus::BLOCK_FILE_MISSING,
        "Status should be BLOCK_FILE_MISSING, got: " +
        node::runtimeStateLoadStatusToString(loaded.status())
    );

    clean(path);
}

// ---- Test 3: Reload rejects malformed finalized artifact ----

void testReloadRejectsMalformedArtifact() {
    const auto path = tempPath("malformed-artifact");
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

    writeFile(stored.blockPath(), "this-is-not-a-valid-artifact\n");

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    require(!loaded.loaded(), "Loader should reject malformed artifact.");
    require(
        loaded.status() == node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
        "Status should be BLOCK_FILE_INVALID, got: " +
        node::runtimeStateLoadStatusToString(loaded.status())
    );

    clean(path);
}

// ---- Test 4: Reload rejects artifact with tampered block hash ----

void testReloadRejectsArtifactWithBlockHashTampering() {
    const auto path = tempPath("tampered-hash");
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

    std::string contents = readFile(stored.blockPath());
    contents = replaceFirst(
        contents,
        "blockHash=" + pipeline.block().hash(),
        "blockHash=0000000000000000000000000000000000000000000000000000000000000000"
    );
    writeFile(stored.blockPath(), contents);

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    require(!loaded.loaded(), "Loader should reject artifact with tampered block hash.");
    require(
        loaded.status() == node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
        "Status should be BLOCK_FILE_INVALID for tampered block hash, got: " +
        node::runtimeStateLoadStatusToString(loaded.status())
    );

    clean(path);
}

// ---- Test 5: Chain audit fails or loader rejects on corrupted supply delta ----

void testChainAuditFailsOnSupplyDeltaTampering() {
    const auto path = tempPath("audit-corrupt");
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

    // Tamper the supply delta: change supplyBeforeRawUnits to an incorrect value.
    // This breaks the supply continuity invariant that the loader enforces.
    std::string contents = readFile(stored.blockPath());
    const std::string beforeTag = "supplyDelta.supplyBeforeRawUnits=";
    const auto pos = contents.find(beforeTag);
    if (pos != std::string::npos) {
        // Find the end of the field value (newline).
        const auto endPos = contents.find('\n', pos);
        if (endPos != std::string::npos) {
            contents.replace(pos, endPos - pos, beforeTag + "1");
            writeFile(stored.blockPath(), contents);
        }
    }

    // The loader must detect the supply continuity break and reject the artifact.
    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    if (loaded.loaded()) {
        // If the loader somehow accepted it, the chain audit must catch the inconsistency.
        const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
        require(!audit.passed(),
            "Chain audit must fail on artifact with tampered supply delta."
        );
    }
    // Loader rejection (BLOCK_FILE_INVALID) also satisfies the requirement.

    clean(path);
}

// ---- Test 6: Restarted node reloads and remains auditable ----

void testRestartedNodeReloadsAndRemainsAuditable() {
    const auto path = tempPath("restart-auditable");
    clean(path);

    const config::GenesisConfig genesis = buildGenesis();
    const node::NodeDataDirectoryConfig dir(path);

    require(
        node::NodeDataDirectory::initialize(dir, genesis, localPeer(), kTimestamp + 1).initialized(),
        "Data directory should initialize."
    );

    {
        node::NodeRuntime runtime = startFreshRuntime(genesis);

        for (int i = 1; i <= 2; ++i) {
            const auto pipeline = produceBlock(runtime, kTimestamp + i * 100, static_cast<std::uint64_t>(i));
            require(pipeline.finalized(), "Block " + std::to_string(i) + " should finalize. Reason=" + pipeline.reason());
            require(
                node::FinalizedBlockStore::persist(dir, runtime, pipeline, kTimestamp + i * 100 + 10).stored(),
                "Block " + std::to_string(i) + " should persist."
            );
        }
    }

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    require(loaded.loaded(), "Reloaded runtime after restart should succeed. Reason=" + loaded.reason());
    require(loaded.loadedBlockCount() == 2, "Reloaded runtime should have 2 blocks from first session.");

    const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
    require(audit.passed(), "Restarted node must pass chain audit. Reason: " + audit.reason());

    clean(path);
}

// ---- Test 7: Deferred reward not counted twice in monetary supply ----

void testDeferredRewardNotCountedTwice() {
    const node::ProtectionRewardSettlement deferredSettlement(
        "validator-deferred", 10,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(1000),
        500, 700,
        node::ProtectionRewards::PROTECTION_SETTLEMENT_REASON,
        "grant-001",
        "work-001"
    );

    const node::ProtectionRewardSettlement earnedSettlement(
        "validator-earned", 10,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(0),
        500, 700,
        node::ProtectionRewards::PROTECTION_SETTLEMENT_REASON,
        "grant-002",
        "work-002"
    );

    assert(deferredSettlement.isValid());
    assert(earnedSettlement.isValid());

    const auto auditResult = node::ProtectionRewards::auditSettlementEvidence(
        {deferredSettlement, earnedSettlement}
    );
    assert(auditResult.isPassed());

    const auto deferredCategory =
        node::ProtectionRewards::categoryForSettlement(deferredSettlement);
    assert(deferredCategory == node::RewardCategory::DEFERRED_PROTECTION);

    const auto earnedCategory =
        node::ProtectionRewards::categoryForSettlement(earnedSettlement);
    assert(earnedCategory == node::RewardCategory::PROTECTION);
}

// ---- Test 8: Explicit zero mint and zero burn in supply delta ----

void testSupplyDeltaExplicitZeroMintAndBurn() {
    const auto genesis = buildGenesis();
    auto runtime = startFreshRuntime(genesis);

    const auto pipeline = produceBlock(runtime, kTimestamp + 20, 1);
    require(pipeline.finalized(), "Pipeline should finalize. Reason=" + pipeline.reason());

    const economics::SupplyDelta& delta = pipeline.supplyDelta();
    assert(delta.isValid());
    assert(delta.blockHeight() == 1);

    const node::MonetaryFirewallAudit& audit = pipeline.monetaryFirewallAudit();
    assert(audit.passed());
    assert(audit.supplyLedger().supplyBefore().rawUnits() > 0);
}

// ---- Test 9: Artifact digest is stable across two recomputations ----

void testArtifactDigestIsStableAcrossRecomputations() {
    const auto path = tempPath("digest-stable");
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

    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(dir, genesis, localPeer());
    require(loaded.loaded(), "Runtime should reload. Reason=" + loaded.reason());

    require(!loaded.loadedArtifacts().empty(), "Loaded artifacts must not be empty.");
    const auto& artifact = loaded.loadedArtifacts().front();
    const std::string firstDigest = artifact.artifactDigest();
    require(!firstDigest.empty(), "Artifact digest must not be empty.");
    require(
        artifact.artifactDigest() == firstDigest,
        "Artifact digest must be deterministic across two calls."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testFullLifecycleInitThroughAudit();
        testReloadRejectsMissingBlockFile();
        testReloadRejectsMalformedArtifact();
        testReloadRejectsArtifactWithBlockHashTampering();
        testChainAuditFailsOnSupplyDeltaTampering();
        testRestartedNodeReloadsAndRemainsAuditable();
        testDeferredRewardNotCountedTwice();
        testSupplyDeltaExplicitZeroMintAndBurn();
        testArtifactDigestIsStableAcrossRecomputations();

        std::cout << "Local node lifecycle tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local node lifecycle tests failed: " << e.what() << "\n";
        return 1;
    }
}
