#include "core/BloomFilter.hpp"

#include "crypto/hash.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace nodo::core {

namespace {

// Convert two hex characters to a byte value
std::uint8_t hexByteToByte(char hi, char lo) {
    auto fromHex = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
        return 0;
    };
    return static_cast<std::uint8_t>((fromHex(hi) << 4) | fromHex(lo));
}

} // namespace

BloomFilter::BloomFilter() {
    m_bits.fill(0);
}

void BloomFilter::computePositions(
    const std::string& item,
    std::size_t& pos1,
    std::size_t& pos2,
    std::size_t& pos3
) const {
    // Compute SHA-256 of the item.
    // nodo_hash_string produces 64 lowercase hex characters (32 bytes encoded).
    char hexOutput[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(item.c_str(), hexOutput, sizeof(hexOutput));

    // The hex output encodes 32 bytes. We need 3 × 3 bytes = 9 raw bytes.
    // Extract bytes 0-2, 3-5, 6-8 from the hash (each byte = 2 hex chars).
    auto byteAt = [&](std::size_t byteIndex) -> std::uint8_t {
        const std::size_t hexIndex = byteIndex * 2;
        return hexByteToByte(hexOutput[hexIndex], hexOutput[hexIndex + 1]);
    };

    // Read three 3-byte big-endian uint24 values
    const std::uint32_t v0 =
        (static_cast<std::uint32_t>(byteAt(0)) << 16) |
        (static_cast<std::uint32_t>(byteAt(1)) <<  8) |
        (static_cast<std::uint32_t>(byteAt(2)));

    const std::uint32_t v1 =
        (static_cast<std::uint32_t>(byteAt(3)) << 16) |
        (static_cast<std::uint32_t>(byteAt(4)) <<  8) |
        (static_cast<std::uint32_t>(byteAt(5)));

    const std::uint32_t v2 =
        (static_cast<std::uint32_t>(byteAt(6)) << 16) |
        (static_cast<std::uint32_t>(byteAt(7)) <<  8) |
        (static_cast<std::uint32_t>(byteAt(8)));

    pos1 = static_cast<std::size_t>(v0 % BITS);
    pos2 = static_cast<std::size_t>(v1 % BITS);
    pos3 = static_cast<std::size_t>(v2 % BITS);
}

void BloomFilter::add(const std::string& item) {
    std::size_t pos1, pos2, pos3;
    computePositions(item, pos1, pos2, pos3);

    m_bits[pos1 / 8] |= static_cast<std::uint8_t>(1u << (pos1 % 8));
    m_bits[pos2 / 8] |= static_cast<std::uint8_t>(1u << (pos2 % 8));
    m_bits[pos3 / 8] |= static_cast<std::uint8_t>(1u << (pos3 % 8));
}

bool BloomFilter::contains(const std::string& item) const {
    std::size_t pos1, pos2, pos3;
    computePositions(item, pos1, pos2, pos3);

    return (m_bits[pos1 / 8] & (1u << (pos1 % 8))) != 0 &&
           (m_bits[pos2 / 8] & (1u << (pos2 % 8))) != 0 &&
           (m_bits[pos3 / 8] & (1u << (pos3 % 8))) != 0;
}

BloomFilter BloomFilter::operator|(const BloomFilter& other) const {
    BloomFilter result;
    for (std::size_t i = 0; i < BYTES; ++i) {
        result.m_bits[i] = m_bits[i] | other.m_bits[i];
    }
    return result;
}

bool BloomFilter::isEmpty() const {
    for (const std::uint8_t byte : m_bits) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

std::string BloomFilter::serialize() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const std::uint8_t byte : m_bits) {
        oss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return oss.str();
}

BloomFilter BloomFilter::deserialize(const std::string& hex) {
    BloomFilter filter;

    if (hex.size() != BYTES * 2) {
        // Return empty filter on malformed input rather than throwing
        return filter;
    }

    for (std::size_t i = 0; i < BYTES; ++i) {
        filter.m_bits[i] = hexByteToByte(hex[i * 2], hex[i * 2 + 1]);
    }

    return filter;
}

BloomFilter BloomFilter::buildFromEvents(const std::vector<EventLog>& events) {
    BloomFilter filter;

    for (const EventLog& event : events) {
        filter.add(event.transactionId());
        filter.add(event.primaryAddress());

        if (!event.secondaryAddress().empty()) {
            filter.add(event.secondaryAddress());
        }

        filter.add(eventTypeToString(event.type()));
    }

    return filter;
}

} // namespace nodo::core
