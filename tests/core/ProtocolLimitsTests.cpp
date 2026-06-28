#include "core/Block.hpp"
#include "core/ProtocolLimits.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "p2p/EncryptedPeerChannel.hpp"
#include "p2p/TcpTransportFrameCodec.hpp"
#include "p2p/TransportFrameCodec.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        using namespace nodo;
        require(
            core::Block::MAX_SERIALIZED_BYTES ==
                core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES,
            "Block and protocol serialized-size limits must match."
        );
        require(
            core::Block::MAX_RECORD_PAYLOAD_BYTES ==
                core::ProtocolLimits::MAX_RECORD_PAYLOAD_BYTES,
            "Block record and protocol payload limits must match."
        );
        require(
            p2p::TransportFrameCodec::MAX_FRAME_BYTES ==
                core::ProtocolLimits::MAX_TRANSPORT_FRAME_BYTES &&
            p2p::TcpTransportFrameCodec::MAX_TCP_FRAME_BYTES ==
                core::ProtocolLimits::MAX_TRANSPORT_FRAME_BYTES &&
            p2p::EncryptedPeerChannelCodec::MAX_ENCRYPTED_FRAME_BYTES ==
                core::ProtocolLimits::MAX_TRANSPORT_FRAME_BYTES,
            "All transport codecs must enforce the same frame limit."
        );
        require(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES >
                core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES,
            "Network payloads need bounded proposal and sync overhead."
        );
        require(
            node::NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH == 1,
            "Hex-encoded persistent sync must send one bounded block per batch."
        );
        std::cout << "Protocol limits tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Protocol limits tests FAILED: " << error.what() << "\n";
        return 1;
    }
}
