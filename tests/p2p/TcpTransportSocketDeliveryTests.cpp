#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

using namespace nodo;

std::optional<p2p::TransportMessage> waitForMessage(
    p2p::TcpTransport& transport,
    const std::string& localNodeId
) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        auto received = transport.poll(localNodeId);
        if (received.has_value()) {
            return received;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return std::nullopt;
}

bool waitForDisconnect(
    p2p::TcpTransport& transport,
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        (void)transport.poll(localNodeId);
        if (!transport.connected(localNodeId, remoteNodeId)) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return false;
}

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

    const auto received = waitForMessage(nodeB, "node-b");
    assert(received.has_value());
    assert(received->fromNodeId() == "node-a");
    assert(received->toNodeId() == "node-b");
    assert(received->envelope().payload() == "hello-node-b");
    assert(nodeB.connected("node-b", "node-a"));
    assert(!nodeB.connected("wrong-local-node", "node-a"));

    p2p::NetworkEnvelope replyEnvelope(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        p2p::NetworkMessageType::PONG,
        "node-b",
        1001,
        30,
        "hello-node-a"
    );

    p2p::TransportMessage reply(
        "node-b",
        "node-a",
        replyEnvelope,
        1001
    );

    assert(nodeB.send(reply).success());

    const auto replyReceived = waitForMessage(nodeA, "node-a");
    assert(replyReceived.has_value());
    assert(replyReceived->fromNodeId() == "node-b");
    assert(replyReceived->toNodeId() == "node-a");
    assert(replyReceived->envelope().payload() == "hello-node-a");

    nodeA.closeAll();
    assert(waitForDisconnect(nodeB, "node-b", "node-a"));

    std::cout << "tcp socket delivery tests passed\n";
    return 0;
}
