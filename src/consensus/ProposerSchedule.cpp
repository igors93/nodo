#include "consensus/ProposerSchedule.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>

namespace nodo::consensus {

std::string ProposerSchedule::buildSelectionKey(
    const std::string& chainId,
    std::uint64_t height,
    std::uint64_t round
) {
    std::ostringstream oss;
    oss << "NODO_PROPOSER_SELECTION_V1:"
        << chainId << ":"
        << height << ":"
        << round;
    return oss.str();
}

std::string ProposerSchedule::selectProposer(
    const core::ValidatorRegistry& registry,
    const std::string& chainId,
    std::uint64_t height,
    std::uint64_t round
) {
    const auto addresses = registry.activeValidatorAddresses();
    if (addresses.empty()) {
        return "";
    }

    const std::string key = buildSelectionKey(chainId, height, round);

    char hashBuf[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(key.c_str(), hashBuf, sizeof(hashBuf));
    const std::string hashHex(hashBuf);

    // Use first 8 hex chars as a 32-bit index.
    if (hashHex.size() < 8) {
        return addresses[0];
    }

    std::uint32_t index = 0;
    for (int i = 0; i < 8; ++i) {
        index <<= 4;
        const char c = hashHex[i];
        if (c >= '0' && c <= '9') {
            index |= static_cast<std::uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            index |= static_cast<std::uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            index |= static_cast<std::uint32_t>(c - 'A' + 10);
        }
    }

    return addresses[index % addresses.size()];
}

} // namespace nodo::consensus
