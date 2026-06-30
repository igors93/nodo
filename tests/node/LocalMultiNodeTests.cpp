// Tests for local multi-node configuration.
// Covers: distinct data directories, distinct peer ports, shared genesis,
// separate validator keys, and ability to reload and audit each local node
// independently.

#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/ChainAuditor.hpp"
#include "node/FinalizedBlockStore.hpp"
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
#include <vector>

namespace {

using namespace nodo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900300000;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / ("nodo-local-multinode-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

struct LocalNode {
    std::string id;
    std::filesystem::path dataDir;
    std::string endpoint;
    crypto::KeyPair validatorKey;
    crypto::KeyPair userKey;
};

LocalNode makeLocalNode(int index) {
    const std::string tag = "n" + std::to_string(index);
    return LocalNode{
        tag,
        tempPath(tag),
        "127.0.0.1:" + std::to_string(19700 + index),
        crypto::KeyPair::createDeterministicBls12381KeyPair("multinode-validator-" + tag),
        crypto::KeyPair::createDeterministicEd25519KeyPair("multinode-user-" + tag)
    };
}

config::GenesisConfig buildSharedGenesis(const std::vector<LocalNode>& nodes) {
    std::vector<config::BootstrapValidatorConfig> validators;
    for (const auto& n : nodes) {
        validators.emplace_back(n.validatorKey.publicKey(), 1, 1, "multinode-validator-" + n.id);
    }

    std::vector<config::GenesisAccountConfig> accounts;
    for (const auto& n : nodes) {
        accounts.emplace_back(
            n.userKey.address().value(),
            Amount::fromRawUnits(2000000000000LL),
            0
        );
    }

    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        validators,
        accounts,
        "multinode-shared-genesis"
    );
}

p2p::PeerInfo peerInfo(const LocalNode& n) {
    return p2p::PeerInfo(
        "peer-" + n.id,
        n.endpoint,
        "nodo/0.1",
        0,
        kTimestamp
    );
}

node::NodeRuntime startRuntime(
    const config::GenesisConfig& genesis,
    const LocalNode& node
) {
    const auto start = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peerInfo(node), 16)
    );
    require(start.started(), "Runtime should start for node " + node.id);
    return start.runtime();
}

crypto::Signer signerFor(const LocalNode& n) {
    static const crypto::Bls12381SignatureProvider provider;
    return crypto::Signer(n.validatorKey, provider);
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& runtime,
    const LocalNode& nodeOwner,
    std::int64_t ts,
    std::uint64_t nonce
) {
    static const crypto::Ed25519SignatureProvider userProvider;
    const auto tx = core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "multinode-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce, ts - 10
        ),
        crypto::Signer(nodeOwner.userKey, userProvider),
        runtime.config().genesisConfig().networkParameters().chainId()
    );
    const auto admission = runtime.mutableMempool().admitTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        ts - 9
    );
    require(admission.accepted(), "Transaction must be admitted for node " + nodeOwner.id);
    return node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        runtime,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        signerFor(nodeOwner)
    );
}

// ---- Test 1: Multiple nodes can be initialized with distinct data directories ----

void testMultipleNodesHaveDistinctDataDirectories() {
    const auto node0 = makeLocalNode(0);
    const auto node1 = makeLocalNode(1);
    clean(node0.dataDir);
    clean(node1.dataDir);

    const auto genesis = buildSharedGenesis({node0, node1});

    const node::NodeDataDirectoryConfig dir0(node0.dataDir);
    const node::NodeDataDirectoryConfig dir1(node1.dataDir);

    require(
        node::NodeDataDirectory::initialize(dir0, genesis, peerInfo(node0), kTimestamp + 1).initialized(),
        "Node 0 data directory should initialize."
    );
    require(
        node::NodeDataDirectory::initialize(dir1, genesis, peerInfo(node1), kTimestamp + 1).initialized(),
        "Node 1 data directory should initialize."
    );

    // Directories must be distinct and not interfere with each other.
    require(
        node0.dataDir != node1.dataDir,
        "Nodes must have distinct data directories."
    );
    require(
        std::filesystem::exists(dir0.manifestPath()),
        "Node 0 manifest must exist."
    );
    require(
        std::filesystem::exists(dir1.manifestPath()),
        "Node 1 manifest must exist."
    );
    require(
        dir0.manifestPath() != dir1.manifestPath(),
        "Node manifests must be in distinct locations."
    );

    clean(node0.dataDir);
    clean(node1.dataDir);
}

// ---- Test 2: Multiple nodes share the same genesis ----

void testMultipleNodesShareGenesis() {
    const auto node0 = makeLocalNode(2);
    const auto node1 = makeLocalNode(3);
    clean(node0.dataDir);
    clean(node1.dataDir);

    const auto genesis = buildSharedGenesis({node0, node1});

    const node::NodeDataDirectoryConfig dir0(node0.dataDir);
    const node::NodeDataDirectoryConfig dir1(node1.dataDir);

    require(
        node::NodeDataDirectory::initialize(dir0, genesis, peerInfo(node0), kTimestamp + 1).initialized(),
        "Node 0 should initialize."
    );
    require(
        node::NodeDataDirectory::initialize(dir1, genesis, peerInfo(node1), kTimestamp + 1).initialized(),
        "Node 1 should initialize."
    );

    const auto manifest0 = node::NodeDataDirectory::loadManifest(dir0);
    const auto manifest1 = node::NodeDataDirectory::loadManifest(dir1);

    require(manifest0.loaded(), "Node 0 manifest should load.");
    require(manifest1.loaded(), "Node 1 manifest should load.");

    require(
        manifest0.manifest().genesisConfigId() == manifest1.manifest().genesisConfigId(),
        "Both nodes must share the same genesis config id."
    );
    require(
        manifest0.manifest().chainId() == manifest1.manifest().chainId(),
        "Both nodes must share the same chain id."
    );

    clean(node0.dataDir);
    clean(node1.dataDir);
}

// ---- Test 3: Local peer endpoints use distinct ports ----

void testLocalNodesHaveDistinctPeerPorts() {
    const auto node0 = makeLocalNode(4);
    const auto node1 = makeLocalNode(5);

    require(
        node0.endpoint != node1.endpoint,
        "Nodes must have distinct peer endpoints."
    );

    const auto peer0 = peerInfo(node0);
    const auto peer1 = peerInfo(node1);

    require(peer0.isValid(), "Node 0 peer info must be valid.");
    require(peer1.isValid(), "Node 1 peer info must be valid.");
    require(
        peer0.endpoint() != peer1.endpoint(),
        "Peer endpoints must differ."
    );
}

// ---- Test 4: Each local node has its own validator key ----

void testEachLocalNodeHasOwnValidatorKey() {
    const auto node0 = makeLocalNode(6);
    const auto node1 = makeLocalNode(7);

    require(
        node0.validatorKey.publicKey().serialize() !=
        node1.validatorKey.publicKey().serialize(),
        "Each local node must have a distinct validator key."
    );
}

// Helper: build a single-validator genesis for one node.
// Each node on its own local chain is the sole validator so it can
// produce blocks without requiring votes from other nodes.
config::GenesisConfig buildSoloGenesis(const LocalNode& n) {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            config::BootstrapValidatorConfig(
                n.validatorKey.publicKey(), 1, 1, "solo-validator-" + n.id
            )
        },
        {
            config::GenesisAccountConfig(
                n.userKey.address().value(),
                Amount::fromRawUnits(2000000000000LL),
                0
            )
        },
        "multinode-solo-genesis-" + n.id
    );
}

// ---- Test 5: Each node can produce and reload blocks independently ----
// Each node uses its own solo genesis (single-validator) so it can produce
// blocks without requiring another node's vote. The shared-genesis scenario
// is covered by testMultipleNodesShareGenesis.

void testEachNodeProducesAndReloadsIndependently() {
    const auto node0 = makeLocalNode(8);
    const auto node1 = makeLocalNode(9);
    clean(node0.dataDir);
    clean(node1.dataDir);

    // Each node uses its own single-validator genesis.
    const auto genesis0 = buildSoloGenesis(node0);
    const auto genesis1 = buildSoloGenesis(node1);

    const node::NodeDataDirectoryConfig dir0(node0.dataDir);
    const node::NodeDataDirectoryConfig dir1(node1.dataDir);

    require(
        node::NodeDataDirectory::initialize(dir0, genesis0, peerInfo(node0), kTimestamp + 1).initialized(),
        "Node 0 should initialize."
    );
    require(
        node::NodeDataDirectory::initialize(dir1, genesis1, peerInfo(node1), kTimestamp + 1).initialized(),
        "Node 1 should initialize."
    );

    // Node 0 produces a block.
    {
        auto rt0 = startRuntime(genesis0, node0);
        const auto pipeline = produceBlock(rt0, node0, kTimestamp + 50, 1);
        require(pipeline.finalized(), "Node 0 should finalize block 1. Reason=" + pipeline.reason());
        require(
            node::FinalizedBlockStore::persist(dir0, rt0, pipeline, kTimestamp + 60).stored(),
            "Node 0 block 1 should persist."
        );
    }

    // Node 1 produces its own block independently.
    {
        auto rt1 = startRuntime(genesis1, node1);
        const auto pipeline = produceBlock(rt1, node1, kTimestamp + 70, 1);
        require(pipeline.finalized(), "Node 1 should finalize block 1. Reason=" + pipeline.reason());
        require(
            node::FinalizedBlockStore::persist(dir1, rt1, pipeline, kTimestamp + 80).stored(),
            "Node 1 block 1 should persist."
        );
    }

    // Node 0 reloads from its own data directory.
    const auto loaded0 = node::RuntimeStateLoader::loadFromDataDirectory(
        dir0, genesis0, peerInfo(node0)
    );
    require(loaded0.loaded(),
        "Node 0 should reload. Reason=" + loaded0.reason()
    );
    require(
        loaded0.loadedBlockCount() == 1,
        "Node 0 should have 1 loaded block."
    );

    // Node 1 reloads from its own data directory.
    const auto loaded1 = node::RuntimeStateLoader::loadFromDataDirectory(
        dir1, genesis1, peerInfo(node1)
    );
    require(loaded1.loaded(),
        "Node 1 should reload. Reason=" + loaded1.reason()
    );
    require(
        loaded1.loadedBlockCount() == 1,
        "Node 1 should have 1 loaded block."
    );

    // Both nodes must be auditable after reload.
    const auto audit0 = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded0);
    require(audit0.passed(),
        "Node 0 must pass chain audit after reload. Reason: " + audit0.reason()
    );

    const auto audit1 = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded1);
    require(audit1.passed(),
        "Node 1 must pass chain audit after reload. Reason: " + audit1.reason()
    );

    // The data directories are distinct.
    require(
        loaded0.manifest().latestBlockHeight() == 1,
        "Node 0 must have 1 finalized block after independent production."
    );
    require(
        loaded1.manifest().latestBlockHeight() == 1,
        "Node 1 must have 1 finalized block after independent production."
    );

    // The two nodes have distinct data directory paths.
    require(
        node0.dataDir != node1.dataDir,
        "Nodes must persist to distinct data directories."
    );

    clean(node0.dataDir);
    clean(node1.dataDir);
}

// ---- Test 6: A node cannot load another node's data directory ----

void testNodeRejectsMismatchedDataDirectory() {
    const auto node0 = makeLocalNode(10);
    const auto node1 = makeLocalNode(11);
    clean(node0.dataDir);
    clean(node1.dataDir);

    // Two different genesis configurations (different seeds).
    const config::GenesisConfig genesis0 = config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        { config::BootstrapValidatorConfig(node0.validatorKey.publicKey(), 1, 1, "v-n0") },
        { config::GenesisAccountConfig(node0.userKey.address().value(), Amount::fromRawUnits(1000000000LL), 0) },
        "multinode-genesis-node0-only"
    );

    const config::GenesisConfig genesis1 = config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        { config::BootstrapValidatorConfig(node1.validatorKey.publicKey(), 1, 1, "v-n1") },
        { config::GenesisAccountConfig(node1.userKey.address().value(), Amount::fromRawUnits(1000000000LL), 0) },
        "multinode-genesis-node1-only"
    );

    const node::NodeDataDirectoryConfig dir0(node0.dataDir);

    require(
        node::NodeDataDirectory::initialize(dir0, genesis0, peerInfo(node0), kTimestamp + 1).initialized(),
        "Node 0 should initialize with its genesis."
    );

    // Attempt to load node0's directory with node1's genesis must fail.
    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
        dir0, genesis1, peerInfo(node1)
    );
    require(!loaded.loaded(),
        "Loader must reject data directory initialized with a different genesis."
    );
    require(
        loaded.status() == node::RuntimeStateLoadStatus::GENESIS_MISMATCH,
        "Status must be GENESIS_MISMATCH when genesis configs differ."
    );

    clean(node0.dataDir);
    clean(node1.dataDir);
}

// ---- Test 7: Local nodes are initialized fresh without cross-contamination ----

void testLocalNodesInitializedFreshAreIndependent() {
    const auto node0 = makeLocalNode(12);
    const auto node1 = makeLocalNode(13);
    clean(node0.dataDir);
    clean(node1.dataDir);

    const auto genesis = buildSharedGenesis({node0, node1});

    const node::NodeDataDirectoryConfig dir0(node0.dataDir);
    const node::NodeDataDirectoryConfig dir1(node1.dataDir);

    const auto init0 = node::NodeDataDirectory::initialize(
        dir0, genesis, peerInfo(node0), kTimestamp + 1
    );
    const auto init1 = node::NodeDataDirectory::initialize(
        dir1, genesis, peerInfo(node1), kTimestamp + 1
    );

    require(init0.initialized(), "Node 0 should initialize.");
    require(init1.initialized(), "Node 1 should initialize.");

    // Both manifests should report height 0 on fresh initialization.
    require(
        init0.manifest().latestBlockHeight() == 0,
        "Freshly initialized node 0 should have height 0."
    );
    require(
        init1.manifest().latestBlockHeight() == 0,
        "Freshly initialized node 1 should have height 0."
    );

    clean(node0.dataDir);
    clean(node1.dataDir);
}

} // namespace

int main() {
    try {
        testMultipleNodesHaveDistinctDataDirectories();
        testMultipleNodesShareGenesis();
        testLocalNodesHaveDistinctPeerPorts();
        testEachLocalNodeHasOwnValidatorKey();
        testEachNodeProducesAndReloadsIndependently();
        testNodeRejectsMismatchedDataDirectory();
        testLocalNodesInitializedFreshAreIndependent();

        std::cout << "Local multi-node tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local multi-node tests failed: " << e.what() << "\n";
        return 1;
    }
}
