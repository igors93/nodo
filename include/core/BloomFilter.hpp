#ifndef NODO_CORE_BLOOM_FILTER_HPP
#define NODO_CORE_BLOOM_FILTER_HPP

#include "core/EventLog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * BloomFilter is a 2048-bit probabilistic set used for efficient address and
 * transaction-id membership checks in block receipts.
 *
 * Security principle:
 * A Bloom filter only provides probabilistic membership. False positives are
 * acceptable; false negatives are impossible. Callers must not rely on it for
 * authoritative membership — only for cheap early-out filtering.
 */
class BloomFilter {
public:
    static constexpr std::size_t BITS       = 2048;
    static constexpr std::size_t BYTES      = BITS / 8;
    static constexpr std::size_t HASH_COUNT = 3;

    BloomFilter();

    void add(const std::string& item);
    bool contains(const std::string& item) const;

    // Bitwise OR — merge two filters (union)
    BloomFilter operator|(const BloomFilter& other) const;

    bool isEmpty() const;

    std::string serialize() const;   // lowercase hex string of the 256 bytes
    static BloomFilter deserialize(const std::string& hex);

    // Build from a list of EventLog entries.
    // Adds: primaryAddress, secondaryAddress (if non-empty), transactionId,
    //       eventTypeToString(type).
    static BloomFilter buildFromEvents(const std::vector<EventLog>& events);

private:
    std::array<std::uint8_t, BYTES> m_bits;

    // Derive three independent bit positions from the SHA-256 digest of item.
    // Each position uses 3 bytes of the digest interpreted as a uint24
    // (big-endian) modulo BITS.
    void computePositions(
        const std::string& item,
        std::size_t& pos1,
        std::size_t& pos2,
        std::size_t& pos3
    ) const;
};

} // namespace nodo::core

#endif
