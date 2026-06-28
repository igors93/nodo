#include "p2p/EncryptedPeerTransport.hpp"
#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <thread>

namespace {

using namespace nodo::p2p;

std::optional<TransportMessage> waitForMessage(
    Transport& transport,
    const std::string& localNodeId
) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        const auto message = transport.poll(localNodeId);
        if (message.has_value()) return message;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

TransportMessage message(
    const std::string& from,
    const std::string& to,
    const std::string& payload,
    std::int64_t now
) {
    return TransportMessage(
        from,
        to,
        NetworkEnvelope(
            "nodo-localnet",
            "localnet-chain",
            "1.0.0",
            NetworkMessageType::PING,
            from,
            now,
            30,
            payload
        ),
        now
    );
}

} // namespace

int main() {
    TcpTransport rawA;
    TcpTransport rawB;
    TcpTransport attacker;
    EncryptedPeerTransport nodeA(rawA);
    EncryptedPeerTransport nodeB(rawB);

    assert(rawA.bind("node-a", "127.0.0.1", 0).success());
    assert(rawB.bind("node-b", "127.0.0.1", 0).success());
    assert(attacker.bind("node-a", "127.0.0.1", 0).success());

    rawA.registerPeerEndpoint("node-b", rawB.localEndpoint());
    attacker.registerPeerEndpoint("node-b", rawB.localEndpoint());
    assert(nodeA.connect("node-a", "node-b").success());

    assert(nodeA.establishSession(
        "node-a", "node-b", "authenticated-session-secret", 1000));
    assert(nodeB.establishSession(
        "node-b", "node-a", "authenticated-session-secret", 1000));

    assert(nodeA.send(message(
        "node-a", "node-b", "legitimate-before-attack", 1001)).success());
    const auto first = waitForMessage(nodeB, "node-b");
    assert(first.has_value());
    assert(first->envelope().payload() == "legitimate-before-attack");
    assert(first->hasConnectionId());
    assert(rawB.isConnectionAuthenticated(
        first->connectionId(), "node-a"));

    assert(attacker.connect("node-a", "node-b").success());
    const TransportMessage untrustedHandshake(
        "node-a",
        "node-b",
        NetworkEnvelope(
            "nodo-localnet",
            "localnet-chain",
            "1.0.0",
            NetworkMessageType::PEER_CHALLENGE,
            "node-a",
            1002,
            30,
            "untrusted-handshake"
        ),
        1002
    );
    assert(attacker.send(untrustedHandshake).success());
    const auto candidateHandshake = waitForMessage(nodeB, "node-b");
    assert(candidateHandshake.has_value());
    assert(candidateHandshake->hasConnectionId());
    assert(candidateHandshake->connectionId() != first->connectionId());
    assert(!rawB.isConnectionAuthenticated(
        candidateHandshake->connectionId(), "node-a"));
    assert(rawB.isConnectionAuthenticated(
        first->connectionId(), "node-a"));

    assert(attacker.send(message(
        "node-a", "node-b", "unauthenticated-replacement", 1003)).success());

    for (int attempt = 0;
         attempt < 500 && nodeB.rejectedFrameCount() == 0;
         ++attempt) {
        (void)nodeB.poll("node-b");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(nodeB.rejectedFrameCount() == 1);
    assert(rawB.isConnectionAuthenticated(
        first->connectionId(), "node-a"));

    assert(nodeA.send(message(
        "node-a", "node-b", "legitimate-after-attack", 1004)).success());
    const auto second = waitForMessage(nodeB, "node-b");
    assert(second.has_value());
    assert(second->envelope().payload() == "legitimate-after-attack");
    assert(second->connectionId() == first->connectionId());

    return 0;
}
