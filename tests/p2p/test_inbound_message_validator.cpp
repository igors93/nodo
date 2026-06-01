#include "p2p/InboundMessageValidator.hpp"

#include <cassert>

int main() {
    nodo::p2p::InboundMessageValidator validator(
        nodo::p2p::InboundMessagePolicy(1024, 60, 30, 2)
    );

    nodo::p2p::NetworkEnvelope first(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        nodo::p2p::NetworkMessageType::PEER_HELLO,
        "node-A",
        1000,
        60,
        "hello"
    );

    const auto accepted = validator.validate(
        first,
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        1001
    );

    assert(accepted.accepted());
    assert(validator.seenMessageCount() == 1);

    const auto duplicate = validator.validate(
        first,
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        1002
    );

    assert(duplicate.status() == nodo::p2p::InboundMessageStatus::DUPLICATE_MESSAGE);

    nodo::p2p::NetworkEnvelope wrongChain(
        "nodo-localnet",
        "wrong-chain",
        "nodo/0.1",
        nodo::p2p::NetworkMessageType::PING,
        "node-B",
        1000,
        60,
        "ping"
    );

    const auto rejected = validator.validate(
        wrongChain,
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        1001
    );

    assert(rejected.status() == nodo::p2p::InboundMessageStatus::WRONG_CHAIN);

    return 0;
}
