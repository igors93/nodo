#include "p2p/TransportFrameCodec.hpp"

#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <stdexcept>

namespace nodo::p2p {

std::vector<unsigned char> TransportFrameCodec::encodeFrame(
    const NetworkEnvelope& envelope
) {
    const std::vector<unsigned char> encodedEnvelope =
        serialization::ProtocolMessageCodec::encodeNetworkEnvelope(envelope);

    if (encodedEnvelope.empty() || encodedEnvelope.size() > MAX_FRAME_BYTES) {
        throw std::runtime_error("Network envelope frame size is invalid.");
    }

    serialization::CanonicalWriter writer;
    writer.writeString("NODO_TRANSPORT_FRAME_V1");
    writer.writeBytes(encodedEnvelope);
    return writer.bytes();
}

NetworkEnvelope TransportFrameCodec::decodeFrame(
    const std::vector<unsigned char>& frame
) {
    if (frame.empty() || frame.size() > MAX_FRAME_BYTES + 128) {
        throw std::runtime_error("Transport frame size is invalid.");
    }

    serialization::CanonicalReader reader(frame, MAX_FRAME_BYTES);

    const std::string version = reader.readString();
    if (version != "NODO_TRANSPORT_FRAME_V1") {
        throw std::runtime_error("Unsupported transport frame version.");
    }

    const std::vector<unsigned char> encodedEnvelope = reader.readBytes();
    reader.requireFullyConsumed();

    return serialization::ProtocolMessageCodec::decodeNetworkEnvelope(encodedEnvelope);
}

bool TransportFrameCodec::isValidFrame(
    const std::vector<unsigned char>& frame
) {
    try {
        const NetworkEnvelope envelope = decodeFrame(frame);
        return envelope.isStructurallyValid(MAX_FRAME_BYTES);
    } catch (...) {
        return false;
    }
}

} // namespace nodo::p2p
