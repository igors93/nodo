#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>
#include <string>

using namespace nodo::p2p;

namespace {

PeerMetadata makePeer(
    const std::string& nodeId,
    const std::string& host,
    std::uint16_t port
) {
    return PeerMetadata(
        nodeId,
        PeerEndpoint(host, port),
        "fingerprint-" + nodeId,
        1000,
        1000,
        0,
        false
    );
}

GossipMeshConfig hardenedConfig(const std::string& nodeId) {
    EclipseGuardConfig eclipseConfig = EclipseGuardConfig::defaults();
    eclipseConfig.maxPeersPerSubnet = 1;
    eclipseConfig.maxSingleSubnetFraction = 1.0;
    return GossipMeshConfig(
        nodeId,
        "localnet",
        "chain-localnet",
        "1",
        "test-genesis-v1",
        60,
        2,
        true,
        true,
        eclipseConfig
    );
}

void testEclipseGuardIsAppliedOnPeerRegistration() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport transport(bus);
    GossipMesh mesh(hardenedConfig("node-a"), transport);

    assert(mesh.registerPeer(makePeer("node-b", "10.1.2.3", 19002)).success());
    assert(!mesh.registerPeer(makePeer("node-c", "10.1.2.4", 19003)).success());
    assert(mesh.registerPeer(makePeer("node-d", "10.1.3.4", 19004)).success());
}

void testNonHandshakeMessageRequiresAuthenticatedSession() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport transportA(bus);
    LoopbackTransport transportB(bus);

    GossipMesh meshA(hardenedConfig("node-a"), transportA);
    GossipMesh meshB(hardenedConfig("node-b"), transportB);

    assert(meshA.registerPeer(makePeer("node-b", "10.2.1.2", 19002)).success());
    assert(meshB.registerPeer(makePeer("node-a", "10.2.2.2", 19001)).success());
    assert(transportA.connect("node-a", "node-b").sent());
    assert(transportB.connect("node-b", "node-a").sent());

    const GossipDeliveryReport queued = meshA.sendTo(
        "node-b",
        NetworkMessageType::CHAIN_STATUS,
        "status",
        1000
    );
    assert(queued.acceptedCount() == 0);
    assert(queued.rejectedCount() == 1);

    const NetworkEnvelope envelope(
        "localnet",
        "chain-localnet",
        "1",
        NetworkMessageType::CHAIN_STATUS,
        "node-a",
        1001,
        60,
        "status"
    );
    assert(transportA.send(TransportMessage("node-a", "node-b", envelope, 1001)).sent());

    const GossipDeliveryReport received = meshB.receiveAvailable(1001);
    assert(received.acceptedCount() == 0);
    assert(received.rejectedCount() == 1);
    assert(meshB.inbox().countForType(NetworkMessageType::CHAIN_STATUS) == 0);
    assert(meshB.invalidMessageCountForPeer("node-a") == 1);
}

} // namespace

int main() {
    testEclipseGuardIsAppliedOnPeerRegistration();
    testNonHandshakeMessageRequiresAuthenticatedSession();
    return 0;
}
