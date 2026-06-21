#ifndef NODO_SERIALIZATION_PROTOCOL_MESSAGE_CODEC_HPP
#define NODO_SERIALIZATION_PROTOCOL_MESSAGE_CODEC_HPP

#include "core/Block.hpp"
#include "node/ChainSyncMessages.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * ProtocolMessageCodec is the canonical binary boundary for first-generation
 * network protocol messages. It is intentionally separate from the current
 * human-readable serialize() methods so the project can migrate safely without
 * breaking diagnostics and tests that still use text serialization.
 */
class ProtocolMessageCodec {
public:
    static std::vector<unsigned char> encodeNetworkEnvelope(
        const p2p::NetworkEnvelope& envelope
    );

    static p2p::NetworkEnvelope decodeNetworkEnvelope(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashNetworkEnvelope(
        const p2p::NetworkEnvelope& envelope
    );

    static std::vector<unsigned char> encodeChainStatusMessage(
        const node::ChainStatusMessage& message
    );

    static node::ChainStatusMessage decodeChainStatusMessage(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashChainStatusMessage(
        const node::ChainStatusMessage& message
    );

    static std::vector<unsigned char> encodeBlockLocator(
        const node::BlockLocator& locator
    );

    static node::BlockLocator decodeBlockLocator(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashBlockLocator(
        const node::BlockLocator& locator
    );

    static std::vector<unsigned char> encodeNetworkBlockSyncRequest(
        const node::NetworkBlockSyncRequest& request
    );

    static node::NetworkBlockSyncRequest decodeNetworkBlockSyncRequest(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashNetworkBlockSyncRequest(
        const node::NetworkBlockSyncRequest& request
    );

    static std::vector<unsigned char> encodeBlockList(
        const std::vector<core::Block>& blocks
    );

    static std::vector<core::Block> decodeBlockList(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashBlockList(
        const std::vector<core::Block>& blocks
    );

    static std::vector<unsigned char> encodeRoundAdvanceMessage(
        const node::RoundAdvanceMessage& message
    );

    static node::RoundAdvanceMessage decodeRoundAdvanceMessage(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashRoundAdvanceMessage(
        const node::RoundAdvanceMessage& message
    );
};

} // namespace nodo::serialization

#endif
