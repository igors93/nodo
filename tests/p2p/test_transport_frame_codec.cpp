#include "p2p/TransportFrameCodec.hpp"

#include <cassert>
#include <vector>

using namespace nodo::p2p;

int main() {
    NetworkEnvelope envelope(
        "localnet",
        "chain-localnet",
        "1",
        NetworkMessageType::CHAIN_STATUS,
        "node-a",
        1000,
        60,
        "status-payload"
    );

    const std::vector<unsigned char> frame =
        TransportFrameCodec::encodeFrame(envelope);

    assert(!frame.empty());
    assert(TransportFrameCodec::isValidFrame(frame));

    const NetworkEnvelope decoded =
        TransportFrameCodec::decodeFrame(frame);

    assert(decoded.messageId() == envelope.messageId());
    assert(decoded.payload() == envelope.payload());
    assert(decoded.senderNodeId() == "node-a");

    std::vector<unsigned char> broken = frame;
    broken.push_back(0xff);
    assert(!TransportFrameCodec::isValidFrame(broken));

    return 0;
}
