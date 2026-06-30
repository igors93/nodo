// Integration tests for a local 4-node testnet.
//
// These tests validate that four independent Nodo nodes can:
//   - initialize from a shared genesis
//   - produce multiple blocks in isolation (solo-validator genesis)
//   - reload state from disk
//   - pass chain audit after reload
//   - reach a consistent finalized height
//   - remain isolated from each other's data directories
//
// Block production uses a single-validator (solo) genesis per node because
// multi-validator quorum requires live P2P voting, which is a later phase.
// Shared-genesis tests cover initialization and chain identity only.

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

constexpr int     kNodeCount      = 4;
constexpr int     kBlocksPerNode  = 3;
constexpr int     kBasePort       = 31330;
constexpr std::int64_t kTimestamp = 1900400000;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / ("nodo-testnet-local-fournode-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

struct TestNode {
    std::string id;
    std::filesystem::path dataDir;
    std::string endpoint;
    crypto::KeyPair validatorKey;
    crypto::KeyPair userKey;
};

TestNode makeTestNode(int index) {
    const std::string tag = "n" + std::to_string(index);
    return TestNode{
        tag,
        tempPath(tag),
        "127.0.0.1:" + std::to_string(kBasePort + index),
        crypto::KeyPair::createDeterministicBls12381KeyPair("testnet-fournode-validator-" + tag),
        crypto::KeyPair::createDeterministicEd25519KeyPair("testnet-fournode-user-" + tag)
    };
}

std::vector<TestNode> makeAllNodes() {
    std::vector<TestNode> nodes;
    nodes.reserve(kNodeCount);
    for (int i = 0; i < kNodeCount; ++i) {
        nodes.push_back(makeTestNode(i));
    }
    return nodes;
}

config::GenesisConfig buildSharedGenesis(const std::vector<TestNode>& nodes) {
    std::vector<config::BootstrapValidatorConfig> validators;
    std::vector<config::GenesisAccountConfig> accounts;

    for (const auto& n : nodes) {
        validators.emplace_back(n.validatorKey.publicKey(), 1, 1, "testnet-validator-" + n.id);
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
        "testnet-local-fournode-shared-genesis"
    );
}

config::GenesisConfig buildSoloGenesis(const TestNode& n) {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            config::BootstrapValidatorConfig(
                n.validatorKey.publicKey(), 1, 1, "testnet-solo-validator-" + n.id
            )
        },
        {
            config::GenesisAccountConfig(
                n.userKey.address().value(),
                Amount::fromRawUnits(2000000000000LL),
                0
            )
        },
        "testnet-local-solo-genesis-" + n.id
    );
}

p2p::PeerInfo peerInfo(const TestNode& n) {
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
    const TestNode& node
) {
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peerInfo(node), 16)
    );
    require(result.started(), "Runtime must start for node " + node.id);
    return result.runtime();
}

crypto::Signer signerFor(const TestNode& n) {
    static const crypto::Bls12381SignatureProvider provider;
    return crypto::Signer(n.validatorKey, provider);
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& runtime,
    const TestNode& node,
    std::int64_t ts,
    std::uint64_t nonce
) {
    static const crypto::Ed25519SignatureProvider userProvider;
    const auto tx = core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "testnet-fournode-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce,
            ts - 10
        ),
        crypto::Signer(node.userKey, userProvider),
        runtime.config().genesisConfig().networkParameters().chainId()
    );

    const auto admission = runtime.mutableMempool().admitTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        ts - 9
    );
    require(admission.accepted(),
        "Transaction must be admitted for node " + node.id
    );

    return node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        runtime,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        signerFor(node)
    );
}

// ---- Test 1: All 4 nodes initialize from a shared genesis ------------------

void testFourNodesInitializeFromSharedGenesis() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    const auto genesis = buildSharedGenesis(nodes);

    for (const auto& n : nodes) {
        const node::NodeDataDirectoryConfig dir(n.dataDir);
        const auto init = node::NodeDataDirectory::initialize(
            dir, genesis, peerInfo(n), kTimestamp + 1
        );
        require(init.initialized(),
            "Node " + n.id + " must initialize from shared genesis."
        );
        require(
            init.manifest().genesisConfigId() == genesis.deterministicId(),
            "Node " + n.id + " genesis id must match shared genesis."
        );
        require(
            init.manifest().latestBlockHeight() == 0,
            "Node " + n.id + " must start at height 0."
        );
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

// ---- Test 2: All 4 nodes have distinct data directories and endpoints -------

void testFourNodesHaveDistinctDataDirsAndEndpoints() {
    const auto nodes = makeAllNodes();

    for (int i = 0; i < kNodeCount; ++i) {
        for (int j = i + 1; j < kNodeCount; ++j) {
            require(
                nodes[i].dataDir != nodes[j].dataDir,
                "Nodes " + nodes[i].id + " and " + nodes[j].id
                + " must have distinct data directories."
            );
            require(
                nodes[i].endpoint != nodes[j].endpoint,
                "Nodes " + nodes[i].id + " and " + nodes[j].id
                + " must have distinct endpoints."
            );
            require(
                nodes[i].validatorKey.publicKey().serialize() !=
                nodes[j].validatorKey.publicKey().serialize(),
                "Nodes " + nodes[i].id + " and " + nodes[j].id
                + " must have distinct validator keys."
            );
        }
    }
}

// ---- Test 3: Shared genesis id is identical on all 4 nodes -----------------

void testSharedGenesisIdConsistentAcrossAllNodes() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    const auto genesis = buildSharedGenesis(nodes);

    std::string firstGenesisId;
    std::string firstChainId;

    for (const auto& n : nodes) {
        const node::NodeDataDirectoryConfig dir(n.dataDir);
        require(
            node::NodeDataDirectory::initialize(dir, genesis, peerInfo(n), kTimestamp + 1)
                .initialized(),
            "Node " + n.id + " must initialize."
        );
        const auto loaded = node::NodeDataDirectory::loadManifest(dir);
        require(loaded.loaded(), "Manifest must load for node " + n.id);

        if (firstGenesisId.empty()) {
            firstGenesisId = loaded.manifest().genesisConfigId();
            firstChainId   = loaded.manifest().chainId();
        } else {
            require(
                loaded.manifest().genesisConfigId() == firstGenesisId,
                "Node " + n.id + " genesis id must match node n0."
            );
            require(
                loaded.manifest().chainId() == firstChainId,
                "Node " + n.id + " chain id must match node n0."
            );
        }
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

// ---- Test 4: Each node produces kBlocksPerNode blocks independently ---------

void testFourNodesProduceBlocksIndependently() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    for (const auto& n : nodes) {
        const auto genesis = buildSoloGenesis(n);
        const node::NodeDataDirectoryConfig dir(n.dataDir);

        require(
            node::NodeDataDirectory::initialize(dir, genesis, peerInfo(n), kTimestamp + 1)
                .initialized(),
            "Node " + n.id + " must initialize."
        );

        auto rt = startRuntime(genesis, n);

        for (int b = 1; b <= kBlocksPerNode; ++b) {
            const std::int64_t ts = kTimestamp + (b * 100);
            const auto result = produceBlock(rt, n, ts, static_cast<std::uint64_t>(b));
            require(result.finalized(),
                "Node " + n.id + " must finalize block " + std::to_string(b)
                + ". Reason=" + result.reason()
            );
            require(
                node::FinalizedBlockStore::persist(dir, rt, result, ts + 10).stored(),
                "Node " + n.id + " must persist block " + std::to_string(b)
            );
        }
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

// ---- Test 5: All 4 nodes reload and audit after kBlocksPerNode blocks -------

void testFourNodesReloadAndAuditAfterBlocks() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    // Produce blocks on all 4 nodes.
    for (const auto& n : nodes) {
        const auto genesis = buildSoloGenesis(n);
        const node::NodeDataDirectoryConfig dir(n.dataDir);

        require(
            node::NodeDataDirectory::initialize(dir, genesis, peerInfo(n), kTimestamp + 1)
                .initialized(),
            "Node " + n.id + " must initialize."
        );

        auto rt = startRuntime(genesis, n);
        for (int b = 1; b <= kBlocksPerNode; ++b) {
            const std::int64_t ts = kTimestamp + (b * 100);
            const auto result = produceBlock(rt, n, ts, static_cast<std::uint64_t>(b));
            require(result.finalized(),
                "Node " + n.id + " block " + std::to_string(b) + " must finalize."
            );
            require(
                node::FinalizedBlockStore::persist(dir, rt, result, ts + 10).stored(),
                "Node " + n.id + " block " + std::to_string(b) + " must persist."
            );
        }
    }

    // Reload and audit all 4 nodes.
    for (const auto& n : nodes) {
        const auto genesis = buildSoloGenesis(n);
        const node::NodeDataDirectoryConfig dir(n.dataDir);

        const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
            dir, genesis, peerInfo(n)
        );
        require(loaded.loaded(),
            "Node " + n.id + " must reload. Reason=" + loaded.reason()
        );
        require(
            loaded.loadedBlockCount() == kBlocksPerNode,
            "Node " + n.id + " must have " + std::to_string(kBlocksPerNode)
            + " loaded blocks, got "
            + std::to_string(loaded.loadedBlockCount())
        );
        require(
            loaded.manifest().latestBlockHeight() == static_cast<std::uint64_t>(kBlocksPerNode),
            "Node " + n.id + " must be at height " + std::to_string(kBlocksPerNode)
        );

        const auto audit = node::ChainAuditor::auditLoadedRuntimeDevMode(loaded);
        require(audit.passed(),
            "Node " + n.id + " must pass chain audit. Reason: " + audit.reason()
        );
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

// ---- Test 6: All 4 nodes reach the same finalized height -------------------

void testFourNodesReachSameFinalizedHeight() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    for (const auto& n : nodes) {
        const auto genesis = buildSoloGenesis(n);
        const node::NodeDataDirectoryConfig dir(n.dataDir);

        require(
            node::NodeDataDirectory::initialize(dir, genesis, peerInfo(n), kTimestamp + 1)
                .initialized(),
            "Node " + n.id + " must initialize."
        );

        auto rt = startRuntime(genesis, n);
        for (int b = 1; b <= kBlocksPerNode; ++b) {
            const std::int64_t ts = kTimestamp + (b * 100);
            const auto result = produceBlock(rt, n, ts, static_cast<std::uint64_t>(b));
            require(result.finalized(),
                "Node " + n.id + " block " + std::to_string(b) + " must finalize."
            );
            require(
                node::FinalizedBlockStore::persist(dir, rt, result, ts + 10).stored(),
                "Node " + n.id + " block " + std::to_string(b) + " must persist."
            );
        }
    }

    std::uint64_t expectedHeight = static_cast<std::uint64_t>(kBlocksPerNode);
    for (const auto& n : nodes) {
        const auto genesis = buildSoloGenesis(n);
        const node::NodeDataDirectoryConfig dir(n.dataDir);
        const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
            dir, genesis, peerInfo(n)
        );
        require(loaded.loaded(),
            "Node " + n.id + " must reload."
        );
        require(
            loaded.manifest().latestBlockHeight() == expectedHeight,
            "Node " + n.id + " must reach height "
            + std::to_string(expectedHeight)
            + ", got "
            + std::to_string(loaded.manifest().latestBlockHeight())
        );
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

// ---- Test 7: A node rejects loading another node's data directory ----------

void testNodeRejectsCrossNodeDataDirectory() {
    const auto nodes = makeAllNodes();
    clean(nodes[0].dataDir);
    clean(nodes[1].dataDir);

    const auto genesis0 = buildSoloGenesis(nodes[0]);
    const auto genesis1 = buildSoloGenesis(nodes[1]);

    const node::NodeDataDirectoryConfig dir0(nodes[0].dataDir);
    require(
        node::NodeDataDirectory::initialize(dir0, genesis0, peerInfo(nodes[0]), kTimestamp + 1)
            .initialized(),
        "Node n0 must initialize."
    );

    // Attempt to load n0's directory with n1's genesis must be rejected.
    const auto loaded = node::RuntimeStateLoader::loadFromDataDirectory(
        dir0, genesis1, peerInfo(nodes[1])
    );
    require(!loaded.loaded(),
        "Loader must reject directory initialized with a different genesis."
    );
    require(
        loaded.status() == node::RuntimeStateLoadStatus::GENESIS_MISMATCH,
        "Status must be GENESIS_MISMATCH."
    );

    clean(nodes[0].dataDir);
    clean(nodes[1].dataDir);
}

// ---- Test 8: Fresh initialization leaves all 4 nodes at height 0 -----------

void testFourNodesFreshInitAtHeightZero() {
    const auto nodes = makeAllNodes();
    for (const auto& n : nodes) clean(n.dataDir);

    const auto genesis = buildSharedGenesis(nodes);

    for (const auto& n : nodes) {
        const node::NodeDataDirectoryConfig dir(n.dataDir);
        const auto init = node::NodeDataDirectory::initialize(
            dir, genesis, peerInfo(n), kTimestamp + 1
        );
        require(init.initialized(),
            "Node " + n.id + " must initialize."
        );
        require(
            init.manifest().latestBlockHeight() == 0,
            "Node " + n.id + " must start at height 0."
        );
    }

    for (const auto& n : nodes) clean(n.dataDir);
}

} // namespace

int main() {
    try {
        testFourNodesInitializeFromSharedGenesis();
        testFourNodesHaveDistinctDataDirsAndEndpoints();
        testSharedGenesisIdConsistentAcrossAllNodes();
        testFourNodesProduceBlocksIndependently();
        testFourNodesReloadAndAuditAfterBlocks();
        testFourNodesReachSameFinalizedHeight();
        testNodeRejectsCrossNodeDataDirectory();
        testFourNodesFreshInitAtHeightZero();

        std::cout << "Testnet local 4-node integration tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Testnet local 4-node integration tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
