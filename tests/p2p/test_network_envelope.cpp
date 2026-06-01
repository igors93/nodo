#include "p2p/NetworkEnvelope.hpp"

#include <cassert>
#include <string>

int main() {
    nodo::p2p::NetworkEnvelope envelope(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        nodo::p2p::NetworkMessageType::PING,
        "node-A",
        1000,
        60,
        "hello"
    );

    assert(envelope.isStructurallyValid(1024));
    assert(envelope.payloadHashMatches());
    assert(!envelope.expiredAt(1010));
    assert(envelope.expiredAt(2000));
    assert(nodo::p2p::networkMessageTypeFromString("PING") == nodo::p2p::NetworkMessageType::PING);
    assert(nodo::p2p::networkMessageTypeToString(envelope.messageType()) == "PING");
    assert(!envelope.messageId().empty());
    assert(envelope.serialize().find("NetworkEnvelope") != std::string::npos);

    return 0;
}
