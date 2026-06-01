#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>

using namespace nodo::p2p;

int main() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport nodeA(bus);
    LoopbackTransport nodeB(bus);

    assert(nodeA.connect("node-a", "node-b").sent());
    assert(nodeB.connect("node-b", "node-a").sent());
    assert(nodeA.connected("node-a", "node-b"));

    NetworkEnvelope envelope(
        "localnet",
        "chain-localnet",
        "1",
        NetworkMessageType::PING,
        "node-a",
        1000,
        60,
        "hello"
    );

    TransportMessage message("node-a", "node-b", envelope, 1000);
    assert(message.isValid());
    assert(nodeA.send(message).sent());

    auto received = nodeB.poll("node-b");
    assert(received.has_value());
    assert(received->fromNodeId() == "node-a");
    assert(received->toNodeId() == "node-b");
    assert(received->envelope().payload() == "hello");
    assert(!nodeB.poll("node-b").has_value());

    return 0;
}
