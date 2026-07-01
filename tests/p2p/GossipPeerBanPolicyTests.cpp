#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>

using namespace nodo::p2p;

namespace {

PeerMetadata makePeer(const std::string& nodeId) {
    return PeerMetadata(
        nodeId,
        PeerEndpoint("127.0.0.1", 30333),
        "fingerprint-" + nodeId,
        1000,
        1000,
        0,
        false
    );
}

GossipMeshConfig config(const std::string& nodeId) {
    return GossipMeshConfig(
        nodeId,
        "localnet",
        "chain-localnet",
        "1",
        "genesis-v1",
        60,
        2,
        false,
        false,
        EclipseGuardConfig::defaults(),
        60
    );
}

} // namespace

int main() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport transport(bus);
    GossipMesh mesh(config("node-a"), transport);
    assert(mesh.registerPeer(makePeer("node-b")).success());
    assert(!mesh.peerBannedAt("node-b", 1000));

    NetworkEnvelope invalid(
        "wrongnet",
        "chain-localnet",
        "1",
        NetworkMessageType::CHAIN_STATUS,
        "node-b",
        1000,
        60,
        "bad-status"
    );
    mesh.reportPeerMisbehavior(
        invalid,
        PeerMisbehaviorType::INVALID_MESSAGE,
        "test invalid message one",
        1000
    );
    assert(!mesh.peerBannedAt("node-b", 1001));
    mesh.reportPeerMisbehavior(
        invalid,
        PeerMisbehaviorType::INVALID_MESSAGE,
        "test invalid message two",
        1002
    );
    assert(mesh.peerBannedAt("node-b", 1003));
    const PeerMetadata* banned = mesh.peerRegistry().peer("node-b");
    assert(banned != nullptr);
    assert(banned->quarantined());
    assert(banned->bannedUntil() == 1062);
    assert(banned->banReason() == "test_invalid_message_two");
    assert(!mesh.connectPeer("node-b").sent());

    assert(mesh.liftExpiredPeerPenalties(1061) == 0);
    assert(mesh.liftExpiredPeerPenalties(1062) == 1);
    assert(!mesh.peerBannedAt("node-b", 1062));
    const PeerMetadata* lifted = mesh.peerRegistry().peer("node-b");
    assert(lifted != nullptr);
    assert(!lifted->quarantined());
    assert(lifted->bannedUntil() == 0);
    return 0;
}
