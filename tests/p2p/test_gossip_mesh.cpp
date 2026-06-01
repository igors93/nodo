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

    GossipMeshConfig configA("node-a", "localnet", "chain-localnet", "1", 60, 2);
    GossipMeshConfig configB("node-b", "localnet", "chain-localnet", "1", 60, 2);

    GossipMesh meshA(configA, transportA);
    GossipMesh meshB(configB, transportB);

    assert(meshA.registerPeer(makePeer("node-b", 19002)).success());
    assert(meshB.registerPeer(makePeer("node-a", 19001)).success());

    assert(meshA.connectPeer("node-b").sent());
    assert(meshB.connectPeer("node-a").sent());

    const GossipDeliveryReport queued =
        meshA.broadcast(NetworkMessageType::TRANSACTION_ANNOUNCE, "tx-1", 1000);

    assert(queued.acceptedCount() == 1);
    assert(queued.rejectedCount() == 0);

    const GossipDeliveryReport sent = meshA.flushOutbound(1000);
    assert(sent.acceptedCount() == 1);

    const GossipDeliveryReport received = meshB.receiveAvailable(1001);
    assert(received.acceptedCount() == 1);
    assert(received.rejectedCount() == 0);
    assert(meshB.inbox().countForType(NetworkMessageType::TRANSACTION_ANNOUNCE) == 1);
    assert(meshB.inbox().messagesForType(NetworkMessageType::TRANSACTION_ANNOUNCE)[0].payload() == "tx-1");

    return 0;
}
