#ifndef NODO_P2P_TCP_TRANSPORT_FRAME_CODEC_HPP
#define NODO_P2P_TCP_TRANSPORT_FRAME_CODEC_HPP

#include "p2p/Transport.hpp"

#include <cstddef>
#include <vector>

namespace nodo::p2p {

/*
 * TcpTransportFrameCodec wraps a routed TransportMessage into canonical bytes.
 *
 * It is intentionally separate from TransportFrameCodec, which wraps only a
 * NetworkEnvelope. TCP delivery needs the routing fields as well: fromNodeId,
 * toNodeId and sentAt.
 */
class TcpTransportFrameCodec {
public:
    static constexpr std::size_t MAX_TCP_FRAME_BYTES = 1024 * 1024;

    static std::vector<unsigned char> encodeTransportMessage(
        const TransportMessage& message
    );

    static TransportMessage decodeTransportMessage(
        const std::vector<unsigned char>& bytes
    );

    static bool isValidTransportMessageFrame(
        const std::vector<unsigned char>& bytes
    );
};

} // namespace nodo::p2p

#endif
