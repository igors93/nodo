#include "node/TcpTestnetNodeRuntime.hpp"

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

    p2p::PeerMetadata peerA(
        "node-a",
        nodeA.transport().localEndpoint(),
        "fingerprint-a",
        1000,
        1000,
        0,
        false
    );

    p2p::PeerMetadata peerB(
        "node-b",
        nodeB.transport().localEndpoint(),
        "fingerprint-b",
        1000,
        1000,
        0,
        false
    );

    assert(nodeA.addPeer(peerB).success());
    assert(nodeB.addPeer(peerA).success());
    assert(nodeA.connectPeer("node-b").success());

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

    const auto broadcastReport = nodeA.broadcastChainStatus(status, 1000);
    assert(broadcastReport.acceptedCount() == 1);

    (void)nodeA.tick(1000);

    for (int attempt = 0;
         attempt < 1000 &&
         nodeB.gossipMesh().inbox().countForType(p2p::NetworkMessageType::CHAIN_STATUS) == 0;
         ++attempt) {
        (void)nodeB.tick(1000);
    }

    assert(nodeB.gossipMesh().inbox().countForType(
        p2p::NetworkMessageType::CHAIN_STATUS
    ) == 1);

    const p2p::NetworkEnvelope invalidSyncMessage(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST,
        "node-b",
        1010,
        30,
        "malformed-sync-request"
    );
    for (std::int64_t offset = 0; offset < 3; ++offset) {
        nodeA.gossipMesh().reportPeerMisbehavior(
            invalidSyncMessage,
            p2p::PeerMisbehaviorType::INVALID_MESSAGE,
            "Slashing evidence request payload is malformed.",
            1010 + offset
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

    nodeA.stop();
    nodeB.stop();
    std::filesystem::remove_all(root);

    std::cout << "tcp testnet node runtime tests passed\n";
    return 0;
}
