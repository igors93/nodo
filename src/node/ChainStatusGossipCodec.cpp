#include "node/ChainStatusGossipCodec.hpp"

#include "crypto/Hex.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <vector>

namespace nodo::node {

namespace {

constexpr const char* CANONICAL_PAYLOAD_PREFIX =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

} // namespace

std::string ChainStatusGossipCodec::encode(
    const ChainStatusMessage& status
) {
    if (!status.isValid()) {
        return {};
    }

    try {
        return std::string(CANONICAL_PAYLOAD_PREFIX) + crypto::hexEncode(
            serialization::ProtocolMessageCodec::encodeChainStatusMessage(
                status
            )
        );
    } catch (...) {
        return {};
    }
}

std::optional<ChainStatusMessage> ChainStatusGossipCodec::decode(
    const std::string& payload
) {
    const std::string prefix(CANONICAL_PAYLOAD_PREFIX);
    if (payload.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    const std::string encoded = payload.substr(prefix.size());
    if (!crypto::isHexString(encoded)) {
        return std::nullopt;
    }

    try {
        ChainStatusMessage status =
            serialization::ProtocolMessageCodec::decodeChainStatusMessage(
                crypto::hexDecode(encoded)
            );
        if (!status.isValid()) {
            return std::nullopt;
        }
        return status;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace nodo::node
