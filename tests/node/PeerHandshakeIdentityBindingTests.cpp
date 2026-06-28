#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "p2p/LoopbackTransport.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>
#include <memory>
#include <string>

namespace {

using namespace nodo;

p2p::PeerMetadata peer(
    const std::string& nodeId,
    const std::string& fingerprint,
    std::int64_t now
) {
    return p2p::PeerMetadata(
        nodeId,
        p2p::PeerEndpoint("127.0.0.1", 19002),
        fingerprint,
        now,
        now,
        0,
        false
    );
}

node::ChainStatusMessage chainStatus() {
    return node::ChainStatusMessage(
        "localnet",
        "chain-localnet",
        "1",
        10,
        "block-10",
        10,
        "block-10"
    );
}

void deliverHello(
    p2p::LoopbackTransport& remoteTransport,
    p2p::GossipMesh& localMesh,
    const p2p::GossipMeshConfig& remoteConfig,
    const p2p::PeerMetadata& remotePeer,
    std::int64_t now
) {
    assert(remoteTransport.connect(
        remotePeer.nodeId(),
        localMesh.config().localNodeId()
    ).success());
    const p2p::NetworkEnvelope hello =
        p2p::PeerHandshakeManager::createHelloEnvelope(
            remoteConfig,
            remotePeer,
            chainStatus(),
            now
        );
    assert(remoteTransport.send(p2p::TransportMessage(
        remotePeer.nodeId(),
        localMesh.config().localNodeId(),
        hello,
        now
    )).success());
    assert(localMesh.receiveAvailable(now).acceptedCount() == 1);
}

} // namespace

int main() {
    auto bus = std::make_shared<p2p::LoopbackTransportBus>();
    p2p::LoopbackTransport localTransport(bus);
    p2p::LoopbackTransport remoteTransport(bus);
    p2p::GossipMesh localMesh(
        p2p::GossipMeshConfig(
            "node-local",
            "localnet",
            "chain-localnet",
            "1",
            "genesis-localnet",
            60,
            3
        ),
        localTransport
    );

    const p2p::GossipMeshConfig originalConfig(
        "node-remote",
        "localnet",
        "chain-localnet",
        "1",
        "genesis-localnet",
        60,
        3
    );
    const p2p::PeerMetadata original =
        peer("node-remote", "fingerprint-remote", 1000);
    deliverHello(
        remoteTransport,
        localMesh,
        originalConfig,
        original,
        1000
    );
    const auto registered = node::PeerHandshakeAutoRegistrar::processInbox(
        localMesh,
        chainStatus(),
        1000
    );
    assert(registered.size() == 1);
    assert(registered[0].registered);

    const p2p::PeerMetadata refreshed =
        peer("node-remote", "FINGERPRINT-REMOTE", 1001);
    deliverHello(
        remoteTransport,
        localMesh,
        originalConfig,
        refreshed,
        1001
    );
    const auto refreshedResult =
        node::PeerHandshakeAutoRegistrar::processInbox(
            localMesh,
            chainStatus(),
            1001
        );
    assert(refreshedResult.size() == 1);
    assert(!refreshedResult[0].registered);
    assert(refreshedResult[0].alreadyKnown);

    const p2p::PeerMetadata takeover =
        peer("node-remote", "fingerprint-attacker", 1002);
    deliverHello(
        remoteTransport,
        localMesh,
        originalConfig,
        takeover,
        1002
    );
    const auto takeoverResult =
        node::PeerHandshakeAutoRegistrar::processInbox(
            localMesh,
            chainStatus(),
            1002
        );
    assert(takeoverResult.size() == 1);
    assert(!takeoverResult[0].registered);
    assert(!takeoverResult[0].alreadyKnown);
    assert(localMesh.peerRegistry().peer("node-remote") != nullptr);
    assert(localMesh.peerRegistry().peer("node-remote")
               ->publicKeyFingerprint() == "fingerprint-remote");

    const p2p::GossipMeshConfig aliasConfig(
        "node-remote-rotated",
        "localnet",
        "chain-localnet",
        "1",
        "genesis-localnet",
        60,
        3
    );
    const p2p::PeerMetadata alias =
        peer("node-remote-rotated", "FINGERPRINT-REMOTE", 1003);
    deliverHello(
        remoteTransport,
        localMesh,
        aliasConfig,
        alias,
        1003
    );
    const auto aliasResult = node::PeerHandshakeAutoRegistrar::processInbox(
        localMesh,
        chainStatus(),
        1003
    );
    assert(aliasResult.size() == 1);
    assert(!aliasResult[0].registered);
    assert(!localMesh.peerRegistry().contains("node-remote-rotated"));
    assert(localMesh.peerRegistry().size() == 1);

    const p2p::NetworkEnvelope bypassAttempt(
        "localnet",
        "chain-localnet",
        "1",
        p2p::NetworkMessageType::PING,
        "node-remote-rotated",
        1004,
        60,
        "ping"
    );
    assert(remoteTransport.send(p2p::TransportMessage(
        "node-remote-rotated",
        "node-local",
        bypassAttempt,
        1004
    )).success());
    const p2p::GossipDeliveryReport bypassDelivery =
        localMesh.receiveAvailable(1004);
    assert(bypassDelivery.acceptedCount() == 0);
    assert(bypassDelivery.rejectedCount() == 1);
    assert(localMesh.inbox().countForType(p2p::NetworkMessageType::PING) == 0);

    return 0;
}
