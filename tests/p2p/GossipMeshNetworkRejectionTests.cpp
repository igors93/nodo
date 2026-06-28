#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>

using namespace nodo::p2p;

namespace {

PeerMetadata makePeer(const std::string& nodeId, std::uint16_t port) {
    return PeerMetadata(
        nodeId,
        PeerEndpoint("127.0.0.1", port),
        "fingerprint-" + nodeId,
        1000,
        1000,
        0,
        false
    );
}

} // namespace

int main() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport transportA(bus);
    LoopbackTransport transportB(bus);

    GossipMeshConfig configB("node-b", "localnet", "chain-localnet", "1", "test-genesis-v1", 60, 1);
    GossipMesh meshB(configB, transportB);

    std::size_t persistenceAttempts = 0;
    meshB.setPeerPenaltyPersistenceHandler([&persistenceAttempts]() {
        ++persistenceAttempts;
        if (persistenceAttempts == 1) {
            throw std::runtime_error("temporary peer store failure");
        }
    });

    assert(meshB.registerPeer(makePeer("node-a", 19001)).success());
    assert(transportA.connect("node-a", "node-b").sent());
    assert(transportB.connect("node-b", "node-a").sent());

    NetworkEnvelope wrongNetwork(
        "wrongnet",
        "chain-localnet",
        "1",
        NetworkMessageType::PING,
        "node-a",
        1000,
        60,
        "bad"
    );

    TransportMessage message("node-a", "node-b", wrongNetwork, 1000);
    assert(transportA.send(message).sent());

    const GossipDeliveryReport received = meshB.receiveAvailable(1001);
    assert(received.acceptedCount() == 0);
    assert(received.rejectedCount() == 1);
    assert(meshB.invalidMessageCountForPeer("node-a") == 1);

    const PeerMetadata* peer = meshB.peerRegistry().peer("node-a");
    assert(peer != nullptr);
    assert(peer->quarantined());
    assert(!meshB.peerPenaltyPersistenceHealthy());

    meshB.reportPeerMisbehavior(
        wrongNetwork,
        PeerMisbehaviorType::INVALID_MESSAGE,
        "Repeated invalid synchronization message.",
        1002
    );
    assert(persistenceAttempts == 2);
    assert(meshB.peerPenaltyPersistenceHealthy());
    assert(peer->score() == -10);

    NetworkEnvelope validAfterQuarantine(
        "localnet",
        "chain-localnet",
        "1",
        NetworkMessageType::PING,
        "node-a",
        1003,
        60,
        "ping"
    );
    assert(transportA.send(
        TransportMessage("node-a", "node-b", validAfterQuarantine, 1003)
    ).sent());
    const GossipDeliveryReport quarantinedDelivery =
        meshB.receiveAvailable(1003);
    assert(quarantinedDelivery.acceptedCount() == 0);
    assert(quarantinedDelivery.rejectedCount() == 1);
    assert(meshB.inbox().countForType(NetworkMessageType::PING) == 0);

    return 0;
}
