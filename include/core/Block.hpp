#ifndef NODO_CORE_BLOCK_HPP
#define NODO_CORE_BLOCK_HPP

#include "core/LedgerRecord.hpp"
#include "core/ProtocolLimits.hpp"

#include <cstdint>
#include <optional>
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
    static constexpr std::size_t MAX_RECORDS =
        ProtocolLimits::MAX_BLOCK_RECORDS;
    static constexpr std::size_t MAX_SERIALIZED_BYTES =
        ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES;
    static constexpr std::size_t MAX_RECORD_PAYLOAD_BYTES =
        ProtocolLimits::MAX_RECORD_PAYLOAD_BYTES;

    Block(
        std::uint64_t index,
        std::string previousHash,
        std::vector<LedgerRecord> records,
        std::int64_t timestamp,
        std::string stateRoot = "",
        std::string receiptsRoot = ""
    );

    static Block createGenesisBlock(
        std::vector<LedgerRecord> records,
        std::int64_t timestamp,
        std::string stateRoot = ""
    );

    std::uint64_t index() const;
    const std::string& previousHash() const;
    const std::string& hash() const;
    std::int64_t timestamp() const;
    const std::vector<LedgerRecord>& records() const;
    const std::string& stateRoot() const;
    const std::string& receiptsRoot() const;

    bool isGenesisBlock() const;
    bool isValid(bool requireProtocolCommitments = true) const;

    /*
     * Returns true if `root` is a canonical commitment hash: exactly 64
     * lowercase hex characters, as produced by the SHA-256 hasher used
     * throughout the protocol.  Rejects empty strings, known placeholder
     * labels, and any non-hex input.
     */
    static bool isCanonicalCommitmentRoot(const std::string& root);

    bool hasCanonicalStateRoot() const;
    bool hasCanonicalReceiptsRoot() const;

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

    /*
     * Reconstruct a Block from its serialized form produced by serialize().
     * Returns nullopt if the payload is malformed or hash verification fails.
     */
    static std::optional<Block> deserialize(const std::string& text);

private:
    bool isWithinResourceLimits() const;
    std::uint64_t m_index;
    std::string m_previousHash;
    std::string m_hash;
    std::vector<LedgerRecord> m_records;
    std::int64_t m_timestamp;
    std::string m_stateRoot;
    std::string m_receiptsRoot;

    std::string calculateHash() const;

    static std::string hashString(const std::string& value);
};

} // namespace nodo::core

#endif
