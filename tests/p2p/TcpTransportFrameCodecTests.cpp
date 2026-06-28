#include "p2p/TcpTransportFrameCodec.hpp"

#include <cassert>
#include <iostream>

using namespace nodo;

int main() {
    p2p::NetworkEnvelope envelope(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        p2p::NetworkMessageType::CHAIN_STATUS,
        "node-a",
        1000,
        30,
        "chain-status-payload"
    );

    p2p::TransportMessage message(
        "node-a",
        "node-b",
        envelope,
        1000
    );

    const auto frame =
        p2p::TcpTransportFrameCodec::encodeTransportMessage(message);

    assert(!frame.empty());
    assert(p2p::TcpTransportFrameCodec::isValidTransportMessageFrame(frame));

    const p2p::TransportMessage decoded =
        p2p::TcpTransportFrameCodec::decodeTransportMessage(frame);

    assert(decoded.isValid());
    assert(decoded.fromNodeId() == "node-a");
    assert(decoded.toNodeId() == "node-b");
    assert(decoded.sentAt() == 1000);
    assert(!decoded.hasConnectionId());
    assert(decoded.envelope().messageId() == envelope.messageId());
    assert(decoded.envelope().payload() == "chain-status-payload");

    std::cout << "tcp transport frame codec tests passed\n";
    return 0;
}
