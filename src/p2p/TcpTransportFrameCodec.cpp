#include "p2p/TcpTransportFrameCodec.hpp"

#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <stdexcept>

namespace nodo::p2p {

namespace {

constexpr const char* TCP_TRANSPORT_FRAME_VERSION =
    "NODO_TCP_TRANSPORT_MESSAGE_V1";

} // namespace

std::vector<unsigned char> TcpTransportFrameCodec::encodeTransportMessage(
    const TransportMessage& message
) {
    if (!message.isValid()) {
        throw std::runtime_error("Cannot encode invalid TCP transport message.");
    }

    serialization::CanonicalWriter writer;
    writer.writeString(TCP_TRANSPORT_FRAME_VERSION);
    writer.writeString(message.fromNodeId());
    writer.writeString(message.toNodeId());
    writer.writeInt64(message.sentAt());
    writer.writeBytes(
        serialization::ProtocolMessageCodec::encodeNetworkEnvelope(
            message.envelope()
        )
    );

    return writer.bytes();
}

TransportMessage TcpTransportFrameCodec::decodeTransportMessage(
    const std::vector<unsigned char>& bytes
) {
    if (bytes.empty() || bytes.size() > MAX_TCP_FRAME_BYTES) {
        throw std::runtime_error("TCP transport frame size is invalid.");
    }

    serialization::CanonicalReader reader(bytes, MAX_TCP_FRAME_BYTES);

    const std::string version = reader.readString();
    if (version != TCP_TRANSPORT_FRAME_VERSION) {
        throw std::runtime_error("Unsupported TCP transport frame version.");
    }

    const std::string fromNodeId = reader.readString();
    const std::string toNodeId = reader.readString();
    const std::int64_t sentAt = reader.readInt64();
    const std::vector<unsigned char> envelopeBytes = reader.readBytes();

    p2p::NetworkEnvelope envelope =
        serialization::ProtocolMessageCodec::decodeNetworkEnvelope(
            envelopeBytes
        );

    reader.requireFullyConsumed();

    TransportMessage message(
        fromNodeId,
        toNodeId,
        envelope,
        sentAt
    );

    if (!message.isValid()) {
        throw std::runtime_error("Decoded TCP transport message is invalid.");
    }

    return message;
}

bool TcpTransportFrameCodec::isValidTransportMessageFrame(
    const std::vector<unsigned char>& bytes
) {
    try {
        (void)decodeTransportMessage(bytes);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace nodo::p2p
