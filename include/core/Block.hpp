#ifndef NODO_CORE_BLOCK_HPP
#define NODO_CORE_BLOCK_HPP

#include "core/LedgerRecord.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * Block groups accepted LedgerRecords into a hashable structure.
 *
 * Security principle:
 * A block must be deterministic. The same block data must always produce
 * the same block hash.
 */
class Block {
public:
    Block(
        std::uint64_t index,
        std::string previousHash,
        std::vector<LedgerRecord> records,
        std::int64_t timestamp
    );

    static Block createGenesisBlock(
        std::vector<LedgerRecord> records,
        std::int64_t timestamp
    );

    std::uint64_t index() const;
    const std::string& previousHash() const;
    const std::string& hash() const;
    std::int64_t timestamp() const;
    const std::vector<LedgerRecord>& records() const;

    bool isGenesisBlock() const;
    bool isValid() const;

    /*
     * Deterministic representation of the block header.
     *
     * This payload excludes the final hash field.
     * The hash is calculated from this payload.
     */
    std::string headerPayload() const;

    /*
     * Full deterministic serialization.
     *
     * This includes the final block hash.
     */
    std::string serialize() const;

private:
    std::uint64_t m_index;
    std::string m_previousHash;
    std::string m_hash;
    std::vector<LedgerRecord> m_records;
    std::int64_t m_timestamp;

    std::string calculateHash() const;

    static std::string hashString(const std::string& value);
};

} // namespace nodo::core

#endif