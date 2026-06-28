#include "p2p/EncryptedPeerTransport.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>

using namespace nodo::p2p;

int main() {
    auto bus = std::make_shared<LoopbackTransportBus>();
    LoopbackTransport rawA(bus);
    LoopbackTransport rawB(bus);

    EncryptedPeerTransport encryptedA(rawA);
    EncryptedPeerTransport encryptedB(rawB);

    assert(encryptedA.connect("node-a", "node-b").success());

    NetworkEnvelope envelope(
        "nodo-localnet",
        "nodo-localnet-chain",
        "nodo/1",
        NetworkMessageType::TRANSACTION_ANNOUNCE,
        "node-a",
        120,
        30,
        "tx:abc123"
    );

    TransportMessage plaintext(
        "node-a",
        "node-b",
        envelope,
        121
    );

    assert(!encryptedA.send(plaintext).success());

    NetworkEnvelope challengeEnvelope(
        "nodo-localnet",
        "nodo-localnet-chain",
        "nodo/1",
        NetworkMessageType::PEER_CHALLENGE,
        "node-a",
        120,
        30,
        "challenge"
    );
    assert(encryptedA.send(TransportMessage(
        "node-a", "node-b", challengeEnvelope, 120
    )).success());
    const auto plaintextHandshake = encryptedB.poll("node-b");
    assert(plaintextHandshake.has_value());
    assert(plaintextHandshake->envelope().messageType() ==
           NetworkMessageType::PEER_CHALLENGE);

    assert(encryptedA.stageOutboundSession(
        "node-a", "node-b", "shared-secret", 100));
    assert(!encryptedA.send(plaintext).success());
    assert(encryptedA.activateOutboundSession("node-a", "node-b"));
    assert(encryptedB.establishInboundSession(
        "node-b", "node-a", "shared-secret", 100));

    assert(encryptedA.send(plaintext).success());

    std::optional<TransportMessage> received = encryptedB.poll("node-b");
    assert(received.has_value());
    assert(received->fromNodeId() == "node-a");
    assert(received->toNodeId() == "node-b");
    assert(received->envelope().messageType() == NetworkMessageType::TRANSACTION_ANNOUNCE);
    assert(received->envelope().payload() == "tx:abc123");
    assert(encryptedB.rejectedFrameCount() == 0);

    NetworkEnvelope spoofedEnvelope(
        "nodo-localnet",
        "nodo-localnet-chain",
        "nodo/1",
        NetworkMessageType::PING,
        "node-a",
        122,
        30,
        "spoofed-plaintext"
    );
    assert(rawA.send(TransportMessage(
        "node-a", "node-b", spoofedEnvelope, 122
    )).success());
    assert(!encryptedB.poll("node-b").has_value());
    assert(encryptedB.rejectedFrameCount() == 1);

    return 0;
}
