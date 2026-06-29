#include "consensus/ProposerSchedule.hpp"

#include "crypto/hash.h"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace nodo::consensus {

namespace {

std::uint64_t hexPrefixToUint64(
    const std::string& hashHex
) {
    std::uint64_t value = 0;
    const std::size_t length = hashHex.size() < 16 ? hashHex.size() : 16;
    for (std::size_t i = 0; i < length; ++i) {
        value <<= 4;
        const char c = hashHex[i];
        if (c >= '0' && c <= '9') {
            value |= static_cast<std::uint64_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= static_cast<std::uint64_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= static_cast<std::uint64_t>(c - 'A' + 10);
        }
    }
    return value;
}

} // namespace

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
    const auto addresses = registry.eligibleValidatorAddresses();
    if (addresses.empty()) {
        return "";
    }

    const std::uint64_t totalWeight =
        registry.totalConsensusWeight();
    if (totalWeight == 0) {
        return "";
    }

    const std::string key = buildSelectionKey(chainId, height, round);

    char hashBuf[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(key.c_str(), hashBuf, sizeof(hashBuf));
    const std::string hashHex(hashBuf);

    const std::uint64_t ticket =
        hexPrefixToUint64(hashHex) % totalWeight;

    std::uint64_t cursor = 0;
    for (const std::string& address : addresses) {
        const std::uint64_t weight =
            registry.consensusWeightFor(address);
        if (weight == 0) {
            continue;
        }
        if (std::numeric_limits<std::uint64_t>::max() - cursor < weight) {
            throw std::overflow_error("Proposer schedule weight overflow.");
        }
        cursor += weight;
        if (ticket < cursor) {
            return address;
        }
    }

    return addresses.back();
}

} // namespace nodo::consensus
