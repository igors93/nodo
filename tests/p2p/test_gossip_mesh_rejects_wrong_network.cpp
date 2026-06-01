#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>

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

    GossipMeshConfig configB("node-b", "localnet", "chain-localnet", "1", 60, 1);
    GossipMesh meshB(configB, transportB);

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

    return 0;
}
