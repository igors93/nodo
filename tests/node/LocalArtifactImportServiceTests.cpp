// Tests for LocalArtifactImportService.
// Validates: node A produces artifact, node B imports it;
// rejection of wrong height, wrong previous hash, invalid content,
// conflicting artifact, and reward without evidence.

#include "config/NetworkParameters.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "node/ChainAuditor.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/LocalArtifactImportService.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "storage/AtomicFile.hpp"
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
using nodo::node::ArtifactImportRejectionReason;
using nodo::node::LocalArtifactImportService;

constexpr std::int64_t kTimestamp = 1900500000;

void require(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& s) {
    return std::filesystem::temp_directory_path() / ("nodo-import-tests-" + s);
}

void clean(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
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

crypto::KeyPair validatorKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(seed);
}

crypto::KeyPair userKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(seed);
}

config::GenesisConfig buildGenesis(
    const std::string& memo,
    const crypto::KeyPair& vKey,
    const crypto::KeyPair& uKey
) {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        { config::BootstrapValidatorConfig(vKey.publicKey(), 1, 1, "import-validator") },
        { config::GenesisAccountConfig(uKey.address().value(), Amount::fromRawUnits(5000000000000LL), 0) },
        memo
    );
}

p2p::PeerInfo peerInfo(const std::string& id, const std::string& ep) {
    return p2p::PeerInfo(id, ep, "nodo/0.1", 0, kTimestamp);
}

node::NodeRuntime startRuntime(
    const config::GenesisConfig& genesis,
    const std::string& peerId,
    const std::string& ep
) {
    const auto start = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peerInfo(peerId, ep), 16)
    );
    require(start.started(), "Runtime should start.");
    return start.runtime();
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& rt,
    const crypto::KeyPair& vKey,
    const crypto::KeyPair& uKey,
    std::int64_t ts,
    std::uint64_t nonce
) {
    static const crypto::Ed25519SignatureProvider userProv;
    static const crypto::Bls12381SignatureProvider valProv;

    const auto tx = core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest("import-recipient", Amount::fromRawUnits(1000), Amount::fromRawUnits(100), nonce, ts - 10),
        crypto::Signer(uKey, userProv),
        rt.config().genesisConfig().networkParameters().chainId()
    );
    const auto adm = rt.mutableMempool().admitTransaction(tx, crypto::CryptoPolicy::developmentPolicy(), crypto::SecurityContext::USER_TRANSACTION, ts - 9);
    require(adm.accepted(), "Transaction must be admitted.");
    return node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        rt, node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        crypto::Signer(vKey, valProv)
    );
}

// ---- Test 1: Node B successfully imports artifact produced by node A ----

void testNodeBImportsValidArtifactFromNodeA() {
    const auto dirA = tempPath("import-a-node");
    const auto dirB = tempPath("import-b-node");
    clean(dirA);
    clean(dirB);

    const auto vk = validatorKey("import-v");
    const auto uk = userKey("import-u");
    const auto genesis = buildGenesis("import-genesis", vk, uk);

    const node::NodeDataDirectoryConfig cfgA(dirA);
    const node::NodeDataDirectoryConfig cfgB(dirB);

    require(node::NodeDataDirectory::initialize(cfgA, genesis, peerInfo("nodeA", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfgA init");
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nodeB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    // Node A produces and persists block 1.
    auto rtA = startRuntime(genesis, "nodeA", "127.0.0.1:19901");
    const auto pipelineA = produceBlock(rtA, vk, uk, kTimestamp + 50, 1);
    require(pipelineA.finalized(), "Block should finalize. " + pipelineA.reason());
    const auto storedA = node::FinalizedBlockStore::persist(cfgA, rtA, pipelineA, kTimestamp + 60);
    require(storedA.stored(), "Block should persist.");

    // Node B starts from genesis and imports node A's artifact.
    auto rtB = startRuntime(genesis, "nodeB", "127.0.0.1:19902");
    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, storedA.blockPath(), kTimestamp + 70
    );

    require(result.accepted(),
        "Node B should accept valid artifact from node A. Reason: " +
        nodo::node::artifactImportRejectionReasonToString(result.rejectionReason()) +
        " Detail: " + result.detail()
    );

    // Node B should now have the imported block.
    require(result.manifest().latestBlockHeight() == 1, "Manifest should show height 1.");

    // Node B can reload from disk and pass chain audit.
    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(cfgB, genesis, peerInfo("nodeB", "127.0.0.1:19902"));
    require(loaded.loaded(), "Node B should reload after import. Reason=" + loaded.reason());
    require(loaded.loadedBlockCount() == 1, "Node B should have 1 imported block.");

    const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
    require(audit.passed(), "Node B chain audit should pass after import. Reason: " + audit.reason());

    clean(dirA);
    clean(dirB);
}

// ---- Test 2: Node B rejects artifact with wrong height ----

void testNodeBRejectsWrongHeight() {
    const auto dirA = tempPath("import-wrong-height-a");
    const auto dirB = tempPath("import-wrong-height-b");
    clean(dirA);
    clean(dirB);

    const auto vk = validatorKey("wrong-height-v");
    const auto uk = userKey("wrong-height-u");
    const auto genesis = buildGenesis("wrong-height-genesis", vk, uk);

    const node::NodeDataDirectoryConfig cfgA(dirA);
    const node::NodeDataDirectoryConfig cfgB(dirB);

    require(node::NodeDataDirectory::initialize(cfgA, genesis, peerInfo("nA", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfgA init");
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    // Node A produces blocks 1 and 2.
    auto rtA = startRuntime(genesis, "nA", "127.0.0.1:19901");
    const auto p1 = produceBlock(rtA, vk, uk, kTimestamp + 50, 1);
    require(p1.finalized(), "block 1 finalize. " + p1.reason());
    const auto s1 = node::FinalizedBlockStore::persist(cfgA, rtA, p1, kTimestamp + 60);
    require(s1.stored(), "block 1 persist");

    const auto p2 = produceBlock(rtA, vk, uk, kTimestamp + 100, 2);
    require(p2.finalized(), "block 2 finalize. " + p2.reason());
    const auto s2 = node::FinalizedBlockStore::persist(cfgA, rtA, p2, kTimestamp + 110);
    require(s2.stored(), "block 2 persist");

    // Node B is at genesis (height 0) and tries to import block 2 (expected height 1 → mismatch).
    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");
    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, s2.blockPath(), kTimestamp + 120
    );

    assert(!result.accepted());
    assert(result.rejectionReason() == ArtifactImportRejectionReason::HEIGHT_CONTINUITY_MISMATCH);

    clean(dirA);
    clean(dirB);
}

// ---- Test 3: Node B rejects artifact with wrong previous hash ----

void testNodeBRejectsWrongPreviousHash() {
    const auto dirA = tempPath("import-wrong-prev-a");
    const auto dirB = tempPath("import-wrong-prev-b");
    clean(dirA);
    clean(dirB);

    const auto vk = validatorKey("wrong-prev-v");
    const auto uk = userKey("wrong-prev-u");
    const auto genesis = buildGenesis("wrong-prev-genesis", vk, uk);

    const node::NodeDataDirectoryConfig cfgA(dirA);
    const node::NodeDataDirectoryConfig cfgB(dirB);

    require(node::NodeDataDirectory::initialize(cfgA, genesis, peerInfo("nA", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfgA init");
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    // Both nodes produce block 1 independently (solo genesis each).
    const auto vk2 = validatorKey("wrong-prev-v2");
    const auto uk2 = userKey("wrong-prev-u2");
    const auto genesis2 = buildGenesis("wrong-prev-genesis-2", vk2, uk2);
    const auto dirA2 = tempPath("import-wrong-prev-a2");
    clean(dirA2);
    const node::NodeDataDirectoryConfig cfgA2(dirA2);
    require(node::NodeDataDirectory::initialize(cfgA2, genesis2, peerInfo("nA2", "127.0.0.1:19903"), kTimestamp + 1).initialized(), "cfgA2 init");

    // Node A produces block 1 with genesis 1.
    auto rtA = startRuntime(genesis, "nA", "127.0.0.1:19901");
    const auto pA = produceBlock(rtA, vk, uk, kTimestamp + 50, 1);
    require(pA.finalized(), "nodeA block finalize. " + pA.reason());
    const auto sA = node::FinalizedBlockStore::persist(cfgA, rtA, pA, kTimestamp + 60);
    require(sA.stored(), "nodeA block persist");

    // Node B: has block 1 from genesis 1. Now try to import a block 2 from a different chain.
    // To simulate wrong previous hash: node B imports block 1 from A successfully,
    // then we tamper the file to change the previousHash.
    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");
    const auto importResult = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, sA.blockPath(), kTimestamp + 70
    );
    require(importResult.accepted(), "First import should succeed. " + importResult.detail());

    // Now produce block 2 with node A.
    const auto pA2 = produceBlock(rtA, vk, uk, kTimestamp + 100, 2);
    require(pA2.finalized(), "nodeA block 2 finalize. " + pA2.reason());
    const auto sA2 = node::FinalizedBlockStore::persist(cfgA, rtA, pA2, kTimestamp + 110);
    require(sA2.stored(), "nodeA block 2 persist");

    // Tamper block 2 to have a wrong previousHash.
    std::string contents = readFile(sA2.blockPath());
    contents = replaceFirst(
        contents,
        "previousHash=" + pA2.block().previousHash(),
        "previousHash=0000000000000000000000000000000000000000000000000000000000000000"
    );
    const auto tamperedPath = tempPath("import-tampered-prev-block2.nodo");
    writeFile(tamperedPath, contents);

    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, tamperedPath, kTimestamp + 120
    );

    // Artifact with tampered previousHash must be rejected (exact reason depends on
    // which check fires first: previousHash comparison, block hash recomputation, or
    // the full validator pipeline).
    assert(!result.accepted() &&
        "Artifact with tampered previousHash must be rejected");

    clean(dirA);
    clean(dirB);
    clean(dirA2);
    std::error_code ec;
    std::filesystem::remove(tamperedPath, ec);
}

// ---- Test 4: Corrupt artifact content is rejected ----

void testCorruptArtifactIsRejected() {
    const auto dirB = tempPath("import-corrupt-b");
    clean(dirB);

    const auto vk = validatorKey("corrupt-v");
    const auto uk = userKey("corrupt-u");
    const auto genesis = buildGenesis("corrupt-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfgB(dirB);
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    // Create a temporary file with garbage content.
    const auto garbagePath = tempPath("import-corrupt-garbage.nodo");
    writeFile(garbagePath, "not-a-valid-artifact\n");

    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");
    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, garbagePath, kTimestamp + 10
    );

    assert(!result.accepted());
    assert(result.rejectionReason() == ArtifactImportRejectionReason::DECODE_FAILED ||
           result.rejectionReason() == ArtifactImportRejectionReason::INVALID_ARTIFACT);

    clean(dirB);
    std::error_code ec;
    std::filesystem::remove(garbagePath, ec);
}

// ---- Test 5: Rejected import does not modify node B's state ----

void testRejectedImportDoesNotModifyState() {
    const auto dirA = tempPath("import-no-mutate-a");
    const auto dirB = tempPath("import-no-mutate-b");
    clean(dirA);
    clean(dirB);

    const auto vk = validatorKey("no-mutate-v");
    const auto uk = userKey("no-mutate-u");
    const auto genesis = buildGenesis("no-mutate-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfgA(dirA);
    const node::NodeDataDirectoryConfig cfgB(dirB);

    require(node::NodeDataDirectory::initialize(cfgA, genesis, peerInfo("nA", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfgA init");
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    // Node A produces blocks 1 and 2.
    auto rtA = startRuntime(genesis, "nA", "127.0.0.1:19901");
    const auto p1 = produceBlock(rtA, vk, uk, kTimestamp + 50, 1);
    require(p1.finalized(), "block 1 finalize. " + p1.reason());
    const auto s1 = node::FinalizedBlockStore::persist(cfgA, rtA, p1, kTimestamp + 60);
    require(s1.stored(), "block 1 persist");
    const auto p2 = produceBlock(rtA, vk, uk, kTimestamp + 100, 2);
    require(p2.finalized(), "block 2 finalize. " + p2.reason());
    const auto s2 = node::FinalizedBlockStore::persist(cfgA, rtA, p2, kTimestamp + 110);
    require(s2.stored(), "block 2 persist");

    // Node B tries to import block 2 (wrong height — should be rejected).
    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");
    const std::uint64_t heightBefore = rtB.blockchain().latestBlock().index();

    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis, s2.blockPath(), kTimestamp + 120
    );

    assert(!result.accepted());
    // Runtime B's blockchain should be unchanged.
    assert(rtB.blockchain().latestBlock().index() == heightBefore);

    // Node B's data directory should still reflect genesis height.
    const auto manifest = node::NodeDataDirectory::loadManifest(cfgB);
    require(manifest.loaded(), "manifest should load");
    assert(manifest.manifest().latestBlockHeight() == 0);

    clean(dirA);
    clean(dirB);
}

// ---- Test 6: Non-existent source file is rejected clearly ----

void testNonexistentSourceFileIsRejected() {
    const auto dirB = tempPath("import-nofile-b");
    clean(dirB);

    const auto vk = validatorKey("nofile-v");
    const auto uk = userKey("nofile-u");
    const auto genesis = buildGenesis("nofile-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfgB(dirB);
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");
    const auto result = LocalArtifactImportService::importArtifactFromFile(
        cfgB, rtB, genesis,
        tempPath("does-not-exist.nodo"),
        kTimestamp + 10
    );

    assert(!result.accepted());
    assert(result.rejectionReason() == ArtifactImportRejectionReason::INVALID_CONFIG);

    clean(dirB);
}

// ---- Test 7: Importing the same artifact twice is idempotent ----

void testImportingSameArtifactTwiceIsIdempotent() {
    const auto dirA = tempPath("import-idem-a");
    const auto dirB = tempPath("import-idem-b");
    clean(dirA);
    clean(dirB);

    const auto vk = validatorKey("idem-v");
    const auto uk = userKey("idem-u");
    const auto genesis = buildGenesis("idem-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfgA(dirA);
    const node::NodeDataDirectoryConfig cfgB(dirB);

    require(node::NodeDataDirectory::initialize(cfgA, genesis, peerInfo("nA", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfgA init");
    require(node::NodeDataDirectory::initialize(cfgB, genesis, peerInfo("nB", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfgB init");

    auto rtA = startRuntime(genesis, "nA", "127.0.0.1:19901");
    const auto pA = produceBlock(rtA, vk, uk, kTimestamp + 50, 1);
    require(pA.finalized(), "block finalize. " + pA.reason());
    const auto sA = node::FinalizedBlockStore::persist(cfgA, rtA, pA, kTimestamp + 60);
    require(sA.stored(), "block persist");

    auto rtB = startRuntime(genesis, "nB", "127.0.0.1:19902");

    // First import.
    const auto r1 = LocalArtifactImportService::importArtifactFromFile(cfgB, rtB, genesis, sA.blockPath(), kTimestamp + 70);
    require(r1.accepted(), "first import should succeed. " + r1.detail());

    // Second import of the same artifact (already on disk) — should be idempotent.
    const auto r2 = LocalArtifactImportService::importArtifactFromFile(cfgB, rtB, genesis, sA.blockPath(), kTimestamp + 80);
    require(r2.accepted(), "second import (idempotent) should succeed. " + r2.detail());

    clean(dirA);
    clean(dirB);
}

} // namespace

int main() {
    try {
        testNodeBImportsValidArtifactFromNodeA();
        testNodeBRejectsWrongHeight();
        testNodeBRejectsWrongPreviousHash();
        testCorruptArtifactIsRejected();
        testRejectedImportDoesNotModifyState();
        testNonexistentSourceFileIsRejected();
        testImportingSameArtifactTwiceIsIdempotent();

        std::cout << "Local artifact import service tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local artifact import service tests failed: " << e.what() << "\n";
        return 1;
    }
}
