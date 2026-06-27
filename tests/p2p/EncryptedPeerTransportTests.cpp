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

    assert(encryptedA.establishSession("node-a", "node-b", "shared-secret", 100));
    assert(encryptedB.establishSession("node-b", "node-a", "shared-secret", 100));
    assert(encryptedA.sessionCount() == 1);
    assert(encryptedB.sessionCount() == 1);

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

    assert(encryptedA.send(plaintext).success());

    std::optional<TransportMessage> received = encryptedB.poll("node-b");
    assert(received.has_value());
    assert(received->fromNodeId() == "node-a");
    assert(received->toNodeId() == "node-b");
    assert(received->envelope().messageType() == NetworkMessageType::TRANSACTION_ANNOUNCE);
    assert(received->envelope().payload() == "tx:abc123");
    assert(encryptedB.rejectedFrameCount() == 0);

    return 0;
}
