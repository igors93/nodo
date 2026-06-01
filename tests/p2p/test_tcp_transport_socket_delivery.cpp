#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <iostream>

using namespace nodo;

int main() {
    p2p::TcpTransport nodeA;
    p2p::TcpTransport nodeB;

    assert(nodeA.bind("node-a", "127.0.0.1", 0).success());
    assert(nodeB.bind("node-b", "127.0.0.1", 0).success());

    assert(nodeA.listening());
    assert(nodeB.listening());
    assert(nodeA.localPort() != 0);
    assert(nodeB.localPort() != 0);

    nodeA.registerPeerEndpoint("node-b", nodeB.localEndpoint());
    nodeB.registerPeerEndpoint("node-a", nodeA.localEndpoint());

    assert(nodeA.connect("node-a", "node-b").success());

    p2p::NetworkEnvelope envelope(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        p2p::NetworkMessageType::PING,
        "node-a",
        1000,
        30,
        "hello-node-b"
    );

    p2p::TransportMessage message(
        "node-a",
        "node-b",
        envelope,
        1000
    );

    assert(nodeA.send(message).success());

    std::optional<p2p::TransportMessage> received;
    for (int attempt = 0; attempt < 1000 && !received.has_value(); ++attempt) {
        received = nodeB.poll("node-b");
    }

    assert(received.has_value());
    assert(received->fromNodeId() == "node-a");
    assert(received->toNodeId() == "node-b");
    assert(received->envelope().payload() == "hello-node-b");
    assert(nodeB.connected("node-b", "node-a"));

    std::cout << "tcp socket delivery tests passed\n";
    return 0;
}
