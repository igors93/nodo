#include "node/NodeRuntime.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::KeyPair;
using nodo::crypto::PublicKey;
using nodo::node::LocalPeerManager;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::PeerUpdateStatus;
using nodo::p2p::PeerInfo;
using nodo::p2p::LocalSyncDecision;
using nodo::consensus::ChainForkSummary;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

PublicKey publicKey(
    const std::string& suffix
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "node-runtime-validator-key-" + suffix
    ).publicKey();
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        publicKey(suffix),
        1,
        1,
        "node-runtime-validator-metadata-" + suffix
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator("a"),
            validator("b")
        },
        "node-runtime-genesis"
    );
}

PeerInfo peer(
    const std::string& id,
    std::uint64_t height = 0,
    std::int64_t lastSeen = kTimestamp
) {
    return PeerInfo(
        id,
        "127.0.0.1:9000",
        "nodo/0.1",
        height,
        lastSeen
    );
}

void testNodeRuntimeStartsFromGenesis() {
    const NodeRuntimeConfig config(
        genesisConfig(),
        peer("local-node", 0),
        8
    );

    const auto result =
        NodeRuntimeFactory::startFromGenesis(config);

    requireCondition(
        result.started(),
        "Node runtime should start from valid genesis config."
    );

    requireCondition(
        result.runtime().blockchain().size() == 1U,
        "Runtime blockchain should start at genesis."
    );

    requireCondition(
        result.runtime().validatorRegistry().activeCount() == 2U,
        "Runtime should have bootstrap validator registry."
    );

    requireCondition(
        result.runtime().chainSummary().isValid(),
        "Runtime chain summary should be valid."
    );
}

void testPeerManagerAddUpdateCapacityAndBestPeer() {
    LocalPeerManager manager(2);

    requireCondition(
        manager.addOrUpdatePeer(peer("peer-a", 1)).success(),
        "Peer A should be accepted."
    );

    requireCondition(
        manager.addOrUpdatePeer(peer("peer-b", 3)).success(),
        "Peer B should be accepted."
    );

    requireCondition(
        manager.addOrUpdatePeer(peer("peer-c", 5)).status() == PeerUpdateStatus::CAPACITY_REACHED,
        "Third peer should be rejected by capacity."
    );

    const PeerInfo* best =
        manager.bestSyncPeer(1);

    requireCondition(
        best != nullptr &&
        best->peerId() == "peer-b",
        "Best sync peer should be highest peer above local height."
    );

    requireCondition(
        manager.addOrUpdatePeer(peer("peer-a", 4, kTimestamp + 1)).success(),
        "Peer A update should succeed."
    );

    best = manager.bestSyncPeer(1);

    requireCondition(
        best != nullptr &&
        best->peerId() == "peer-a",
        "Updated Peer A should become best sync peer."
    );
}

void testRuntimeCreatesChainSummaryMessage() {
    const auto start =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                peer("local-node", 0),
                8
            )
        );

    requireCondition(
        start.started(),
        "Runtime should start."
    );

    const auto message =
        start.runtime().localChainSummaryMessage(
            "remote-node",
            kTimestamp + 10
        );

    requireCondition(
        message.isValid(),
        "Runtime should create valid chain summary message."
    );
}

void testRuntimeEvaluatesBetterPeerForSync() {
    const auto start =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                peer("local-node", 0),
                8
            )
        );

    requireCondition(
        start.started(),
        "Runtime should start."
    );

    const ChainForkSummary remoteSummary(
        3,
        2,
        "remote-latest-hash"
    );

    const auto plan =
        start.runtime().evaluatePeerForSync(
            peer("remote-node", 2),
            remoteSummary,
            kTimestamp + 20
        );

    requireCondition(
        plan.decision() == LocalSyncDecision::REQUEST_BLOCKS &&
        plan.shouldRequestBlocks(),
        "Better remote peer should trigger sync request."
    );
}

void testRuntimeRejectsInvalidConfig() {
    const NodeRuntimeConfig invalidConfig(
        GenesisConfig(),
        peer("local-node", 0),
        8
    );

    const auto result =
        NodeRuntimeFactory::startFromGenesis(invalidConfig);

    requireCondition(
        !result.started(),
        "Invalid runtime config should be rejected."
    );
}

} // namespace

int main() {
    try {
        testNodeRuntimeStartsFromGenesis();
        testPeerManagerAddUpdateCapacityAndBestPeer();
        testRuntimeCreatesChainSummaryMessage();
        testRuntimeEvaluatesBetterPeerForSync();
        testRuntimeRejectsInvalidConfig();

        std::cout << "Nodo node runtime tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo node runtime tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
