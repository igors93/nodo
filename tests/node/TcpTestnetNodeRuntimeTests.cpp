#include "node/TcpTestnetNodeRuntime.hpp"
#include "node/ChainStatusGossipCodec.hpp"
#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "crypto/KeyPair.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nodo;

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nodo_tcp_testnet_node_runtime_test";

    std::filesystem::remove_all(root);

    const std::string genesisId = "nodo-localnet-genesis";

    node::TcpTestnetNodeRuntime nodeA(
        node::TcpTestnetNodeRuntimeConfig(
            "node-a",
            "127.0.0.1",
            0,
            "nodo-localnet",
            "localnet-chain",
            "1.0.0",
            genesisId,
            root / "node-a",
            30,
            3
        )
    );

    node::TcpTestnetNodeRuntime nodeB(
        node::TcpTestnetNodeRuntimeConfig(
            "node-b",
            "127.0.0.1",
            0,
            "nodo-localnet",
            "localnet-chain",
            "1.0.0",
            genesisId,
            root / "node-b",
            30,
            3
        )
    );

    assert(nodeA.start().success());
    assert(nodeB.start().success());
    assert(nodeA.running());
    assert(nodeB.running());

    assert(nodeA.connectUnverifiedPeer(
        "node-b",
        nodeB.transport().localEndpoint()
    ).success());
    assert(!nodeA.gossipMesh().peerRegistry().contains("node-b"));

    const crypto::KeyPair identityA =
        crypto::KeyPair::createDeterministicEd25519KeyPair(
            "tcp-runtime-node-a");
    const crypto::KeyPair identityB =
        crypto::KeyPair::createDeterministicEd25519KeyPair(
            "tcp-runtime-node-b");

    p2p::PeerMetadata peerA(
        "node-a",
        nodeA.transport().localEndpoint(),
        identityA.publicKey().fingerprint(),
        1000,
        1000,
        0,
        false
    );

    p2p::PeerMetadata peerB(
        "node-b",
        nodeB.transport().localEndpoint(),
        identityB.publicKey().fingerprint(),
        1000,
        1000,
        0,
        false
    );

    node::ChainStatusMessage status(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        10,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        10,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );

    assert(status.isValid());

    assert(node::PeerHandshakeAutoRegistrar::initiateHandshake(
        nodeA.gossipMesh(), "node-b", 1000
    ).allAccepted());

    for (std::int64_t attempt = 0;
         attempt < 20 &&
         (!nodeA.gossipMesh().peerRegistry().contains("node-b") ||
          !nodeB.gossipMesh().peerRegistry().contains("node-a") ||
          !nodeA.hasAuthenticatedSession("node-b") ||
          !nodeB.hasAuthenticatedSession("node-a"));
         ++attempt) {
        const std::int64_t now = 1000 + attempt;
        (void)nodeB.tick(now);
        (void)node::PeerHandshakeAutoRegistrar::processInbox(
            nodeB.gossipMesh(), peerB, status, identityB, now);
        (void)nodeA.tick(now);
        (void)node::PeerHandshakeAutoRegistrar::processInbox(
            nodeA.gossipMesh(), peerA, status, identityA, now);
    }

    assert(nodeA.gossipMesh().peerRegistry().contains("node-b"));
    assert(nodeB.gossipMesh().peerRegistry().contains("node-a"));
    assert(nodeA.hasAuthenticatedSession("node-b"));
    assert(nodeB.hasAuthenticatedSession("node-a"));

    const auto broadcastReport = nodeA.broadcastChainStatus(status, 3000);
    assert(broadcastReport.acceptedCount() == 1);

    (void)nodeA.tick(3000);

    for (int attempt = 0;
         attempt < 1000 &&
         nodeB.gossipMesh().inbox().countForType(p2p::NetworkMessageType::CHAIN_STATUS) == 0;
         ++attempt) {
        (void)nodeB.tick(3000);
    }

    const auto manualStatuses = nodeB.gossipMesh().drainInbox(
        p2p::NetworkMessageType::CHAIN_STATUS
    );
    assert(!manualStatuses.empty());
    for (const auto& envelope : manualStatuses) {
        assert(node::ChainStatusGossipCodec::decode(
            envelope.payload()
        ).has_value());
    }

    const p2p::NetworkEnvelope invalidSyncMessage(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST,
        "node-b",
        3001,
        30,
        "malformed-sync-request"
    );
    for (std::int64_t offset = 0; offset < 3; ++offset) {
        nodeA.gossipMesh().reportPeerMisbehavior(
            invalidSyncMessage,
            p2p::PeerMisbehaviorType::INVALID_MESSAGE,
            "Slashing evidence request payload is malformed.",
            3001 + offset
        );
    }

    const p2p::PeerMetadata* penalized =
        nodeA.gossipMesh().peerRegistry().peer("node-b");
    assert(penalized != nullptr);
    assert(penalized->score() == -30);
    assert(penalized->quarantined());
    assert(nodeA.gossipMesh().invalidMessageCountForPeer("node-b") == 3);
    assert(nodeA.gossipMesh().peerPenaltyPersistenceHealthy());

    nodeA.savePeersToDisk();
    assert(std::filesystem::exists(nodeA.config().peersFilePath()));

    node::TcpTestnetNodeRuntime reloadedNodeA(
        node::TcpTestnetNodeRuntimeConfig(
            "node-a",
            "127.0.0.1",
            0,
            "nodo-localnet",
            "localnet-chain",
            "1.0.0",
            genesisId,
            root / "node-a",
            30,
            3
        )
    );
    assert(reloadedNodeA.loadPeersFromDisk(2000) == 1);
    const p2p::PeerMetadata* restored =
        reloadedNodeA.gossipMesh().peerRegistry().peer("node-b");
    assert(restored != nullptr);
    assert(restored->score() == -30);
    assert(restored->quarantined());
    assert(reloadedNodeA.gossipMesh().invalidMessageCountForPeer("node-b") == 3);
    assert(!reloadedNodeA.connectPeer("node-b").success());

    p2p::PeerMetadata rotatedIdentity(
        "node-b-rotated",
        nodeB.transport().localEndpoint(),
        identityB.publicKey().fingerprint(),
        2000,
        2000,
        0,
        false
    );
    assert(!reloadedNodeA.addPeer(rotatedIdentity).success());
    assert(!reloadedNodeA.gossipMesh().peerRegistry().contains(
        "node-b-rotated"
    ));
    assert(!reloadedNodeA.transport().hasPeerEndpoint("node-b-rotated"));
    assert(reloadedNodeA.gossipMesh().peerRegistry().peerByIdentityKey(
        restored->identityKey()
    )->quarantined());

    nodeA.stop();
    nodeB.stop();
    std::filesystem::remove_all(root);

    std::cout << "tcp testnet node runtime tests passed\n";
    return 0;
}
