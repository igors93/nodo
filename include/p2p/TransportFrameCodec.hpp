#ifndef NODO_P2P_TRANSPORT_FRAME_CODEC_HPP
#define NODO_P2P_TRANSPORT_FRAME_CODEC_HPP

#include "p2p/NetworkEnvelope.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::p2p {

class TransportFrameCodec {
public:
    static constexpr std::size_t MAX_FRAME_BYTES = 1024 * 1024;

    static std::vector<unsigned char> encodeFrame(
        const NetworkEnvelope& envelope
    );

    static NetworkEnvelope decodeFrame(
        const std::vector<unsigned char>& frame
    );

    static bool isValidFrame(
        const std::vector<unsigned char>& frame
    );
};

} // namespace nodo::p2p

#endif
