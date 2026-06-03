// Tests for LocalNodeStateSummary and LocalNetworkStateInspector.
// Validates: summary from data directory, divergence detection (genesis,
// height, hash, unreadable state), aligned pair detection.

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
#include "node/LocalNetworkStateInspector.hpp"
#include "node/LocalNodeIdentity.hpp"
#include "node/LocalNodeStateSummary.hpp"
#include "node/LocalPeerTopology.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>

namespace {

using namespace nodo;
using nodo::utils::Amount;
using nodo::node::LocalNodeDivergenceKind;
using nodo::node::LocalNodeIdentity;
using nodo::node::LocalNodeStateSummary;
using nodo::node::LocalNetworkStateInspector;
using nodo::node::LocalPeerTopology;

constexpr std::int64_t kTimestamp = 1900400000;

void require(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& s) {
    return std::filesystem::temp_directory_path() / ("nodo-inspector-tests-" + s);
}

void clean(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
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
        { config::BootstrapValidatorConfig(vKey.publicKey(), 1, 1, "inspector-validator") },
        { config::GenesisAccountConfig(uKey.address().value(), Amount::fromRawUnits(2000000000000LL), 0) },
        memo
    );
}

p2p::PeerInfo peerInfo(const std::string& id, const std::string& endpoint) {
    return p2p::PeerInfo(id, endpoint, "nodo/0.1", 0, kTimestamp);
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis, const std::string& peerId) {
    const auto start = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peerInfo(peerId, "127.0.0.1:19900"), 16)
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
        core::TransactionBuildRequest("inspector-recipient", Amount::fromRawUnits(1000), Amount::fromRawUnits(100), nonce, ts - 10),
        crypto::Signer(uKey, userProv)
    );
    const auto adm = rt.mutableMempool().admitTransaction(tx, crypto::CryptoPolicy::developmentPolicy(), crypto::SecurityContext::USER_TRANSACTION, ts - 9);
    require(adm.accepted(), "Transaction must be admitted.");
    return node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        rt,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        crypto::Signer(vKey, valProv)
    );
}

// ---- Test 1: Summary from uninitialized directory is unreadable ----

void testSummaryFromUninitializedDirIsUnreadable() {
    const auto dir = tempPath("uninit");
    clean(dir);

    const LocalNodeStateSummary s = LocalNodeStateSummary::fromDataDirectory(
        node::NodeDataDirectoryConfig(dir), "node-X", "127.0.0.1:19900"
    );

    assert(!s.isReadable());
    assert(!s.readError().empty());
}

// ---- Test 2: Summary from initialized (genesis-only) directory is readable ----

void testSummaryFromInitializedDirIsReadable() {
    const auto dir = tempPath("init-only");
    clean(dir);

    const auto vk = validatorKey("inspector-v");
    const auto uk = userKey("inspector-u");
    const auto genesis = buildGenesis("inspector-genesis", vk, uk);

    require(
        node::NodeDataDirectory::initialize(
            node::NodeDataDirectoryConfig(dir),
            genesis,
            peerInfo("inspector-node", "127.0.0.1:19900"),
            kTimestamp + 1
        ).initialized(),
        "Directory should initialize."
    );

    const LocalNodeStateSummary s = LocalNodeStateSummary::fromDataDirectory(
        node::NodeDataDirectoryConfig(dir), "inspector-node", "127.0.0.1:19900"
    );

    assert(s.isReadable());
    assert(!s.genesisId().empty());
    assert(s.latestHeight() == 0);

    clean(dir);
}

// ---- Test 3: Two identical nodes report no divergence ----

void testIdenticalNodesAreAligned() {
    const auto dir = tempPath("identical-nodes");
    clean(dir);

    const auto vk = validatorKey("identical-v");
    const auto uk = userKey("identical-u");
    const auto genesis = buildGenesis("identical-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfg(dir);

    require(
        node::NodeDataDirectory::initialize(cfg, genesis, peerInfo("node-1", "127.0.0.1:19901"), kTimestamp + 1).initialized(),
        "Directory should initialize."
    );

    const LocalNodeStateSummary s1 = LocalNodeStateSummary::fromDataDirectory(cfg, "node-1", "127.0.0.1:19901");
    const LocalNodeStateSummary s2 = LocalNodeStateSummary::fromDataDirectory(cfg, "node-1", "127.0.0.1:19901");

    const auto report = LocalNetworkStateInspector::compareNodes(s1, s2);
    assert(!report.isDivergent());
    assert(report.kind() == LocalNodeDivergenceKind::NONE);

    clean(dir);
}

// ---- Test 4: Genesis mismatch is detected ----

void testGenesisMismatchDetected() {
    const auto dir1 = tempPath("genesis-a");
    const auto dir2 = tempPath("genesis-b");
    clean(dir1);
    clean(dir2);

    const auto vk1 = validatorKey("genesis-a-v");
    const auto uk1 = userKey("genesis-a-u");
    const auto genesis1 = buildGenesis("genesis-a", vk1, uk1);

    const auto vk2 = validatorKey("genesis-b-v");
    const auto uk2 = userKey("genesis-b-u");
    const auto genesis2 = buildGenesis("genesis-b", vk2, uk2);

    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir1), genesis1, peerInfo("n1", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "dir1 init");
    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir2), genesis2, peerInfo("n2", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "dir2 init");

    const LocalNodeStateSummary s1 = LocalNodeStateSummary::fromDataDirectory(node::NodeDataDirectoryConfig(dir1), "n1", "127.0.0.1:19901");
    const LocalNodeStateSummary s2 = LocalNodeStateSummary::fromDataDirectory(node::NodeDataDirectoryConfig(dir2), "n2", "127.0.0.1:19902");

    const auto report = LocalNetworkStateInspector::compareNodes(s1, s2);
    assert(report.isDivergent());
    assert(report.kind() == LocalNodeDivergenceKind::GENESIS_MISMATCH);

    clean(dir1);
    clean(dir2);
}

// ---- Test 5: Different heights cause divergence ----

void testDifferentHeightsDiverge() {
    const auto dir1 = tempPath("height-a");
    const auto dir2 = tempPath("height-b");
    clean(dir1);
    clean(dir2);

    const auto vk = validatorKey("height-v");
    const auto uk = userKey("height-u");
    const auto genesis = buildGenesis("height-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfg1(dir1);
    const node::NodeDataDirectoryConfig cfg2(dir2);

    require(node::NodeDataDirectory::initialize(cfg1, genesis, peerInfo("n1", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfg1 init");
    require(node::NodeDataDirectory::initialize(cfg2, genesis, peerInfo("n2", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "cfg2 init");

    // node 1 produces a block.
    {
        auto rt = startRuntime(genesis, "n1");
        const auto pipeline = produceBlock(rt, vk, uk, kTimestamp + 50, 1);
        require(pipeline.finalized(), "block should finalize. " + pipeline.reason());
        require(node::FinalizedBlockStore::persist(cfg1, rt, pipeline, kTimestamp + 60).stored(), "block should persist.");
    }

    // node 2 stays at genesis.
    const LocalNodeStateSummary s1 = LocalNodeStateSummary::fromDataDirectory(cfg1, "n1", "127.0.0.1:19901");
    const LocalNodeStateSummary s2 = LocalNodeStateSummary::fromDataDirectory(cfg2, "n2", "127.0.0.1:19902");

    assert(s1.isReadable() && s1.latestHeight() == 1);
    assert(s2.isReadable() && s2.latestHeight() == 0);

    const auto report = LocalNetworkStateInspector::compareNodes(s1, s2);
    assert(report.isDivergent());
    assert(report.kind() == LocalNodeDivergenceKind::DIFFERENT_HEIGHT);

    clean(dir1);
    clean(dir2);
}

// ---- Test 6: Unreadable node causes divergence ----

void testUnreadableNodeCausesDivergence() {
    // Create one readable summary and one from an uninitialized directory.
    const auto dir1 = tempPath("readable-node");
    const auto dir2 = tempPath("unreadable-node-nonexistent");
    clean(dir1);
    clean(dir2);

    const auto vk = validatorKey("readable-v");
    const auto uk = userKey("readable-u");
    const auto genesis = buildGenesis("readable-genesis", vk, uk);
    const node::NodeDataDirectoryConfig cfg1(dir1);

    require(node::NodeDataDirectory::initialize(cfg1, genesis, peerInfo("n1", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "cfg1 init");

    const LocalNodeStateSummary s1 = LocalNodeStateSummary::fromDataDirectory(cfg1, "n1", "127.0.0.1:19901");
    const LocalNodeStateSummary s2 = LocalNodeStateSummary::fromDataDirectory(
        node::NodeDataDirectoryConfig(dir2), "n2", "127.0.0.1:19902"
    );

    assert(s1.isReadable());
    assert(!s2.isReadable());

    const auto report = LocalNetworkStateInspector::compareNodes(s1, s2);
    assert(report.isDivergent());
    assert(report.kind() == LocalNodeDivergenceKind::UNREADABLE_STATE);

    clean(dir1);
}

// ---- Test 7: summarize from topology ----

void testSummarizeFromTopology() {
    const auto dir1 = tempPath("topo-a");
    const auto dir2 = tempPath("topo-b");
    clean(dir1);
    clean(dir2);

    const auto vk = validatorKey("topo-v");
    const auto uk = userKey("topo-u");
    const auto genesis = buildGenesis("topo-genesis", vk, uk);

    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir1), genesis, peerInfo("n1", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "d1 init");
    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir2), genesis, peerInfo("n2", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "d2 init");

    LocalPeerTopology topo;
    require(topo.addNode(LocalNodeIdentity("n1", "127.0.0.1:19901", "seed-n1", dir1, genesis.deterministicId())).isAdded(), "add n1");
    require(topo.addNode(LocalNodeIdentity("n2", "127.0.0.1:19902", "seed-n2", dir2, genesis.deterministicId())).isAdded(), "add n2");

    const auto summaries = LocalNetworkStateInspector::summarize(topo);
    assert(summaries.size() == 2);
    assert(summaries[0].nodeId() == "n1");
    assert(summaries[1].nodeId() == "n2");

    const auto divergences = LocalNetworkStateInspector::findDivergence(summaries);
    assert(divergences.empty());

    clean(dir1);
    clean(dir2);
}

// ---- Test 8: findDivergence detects genesis mismatch in a group ----

void testFindDivergenceDetectsGenesisMismatch() {
    const auto dir1 = tempPath("fdg-a");
    const auto dir2 = tempPath("fdg-b");
    clean(dir1);
    clean(dir2);

    const auto vk1 = validatorKey("fdg-a-v");
    const auto uk1 = userKey("fdg-a-u");
    const auto g1 = buildGenesis("fdg-genesis-a", vk1, uk1);

    const auto vk2 = validatorKey("fdg-b-v");
    const auto uk2 = userKey("fdg-b-u");
    const auto g2 = buildGenesis("fdg-genesis-b", vk2, uk2);

    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir1), g1, peerInfo("n1", "127.0.0.1:19901"), kTimestamp + 1).initialized(), "d1 init");
    require(node::NodeDataDirectory::initialize(node::NodeDataDirectoryConfig(dir2), g2, peerInfo("n2", "127.0.0.1:19902"), kTimestamp + 1).initialized(), "d2 init");

    const std::vector<LocalNodeStateSummary> summaries = {
        LocalNodeStateSummary::fromDataDirectory(node::NodeDataDirectoryConfig(dir1), "n1", "127.0.0.1:19901"),
        LocalNodeStateSummary::fromDataDirectory(node::NodeDataDirectoryConfig(dir2), "n2", "127.0.0.1:19902")
    };

    const auto divergences = LocalNetworkStateInspector::findDivergence(summaries);
    assert(!divergences.empty());
    assert(divergences.front().kind() == LocalNodeDivergenceKind::GENESIS_MISMATCH);

    clean(dir1);
    clean(dir2);
}

} // namespace

int main() {
    try {
        testSummaryFromUninitializedDirIsUnreadable();
        testSummaryFromInitializedDirIsReadable();
        testIdenticalNodesAreAligned();
        testGenesisMismatchDetected();
        testDifferentHeightsDiverge();
        testUnreadableNodeCausesDivergence();
        testSummarizeFromTopology();
        testFindDivergenceDetectsGenesisMismatch();

        std::cout << "Local network state inspector tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local network state inspector tests failed: " << e.what() << "\n";
        return 1;
    }
}
