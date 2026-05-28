#ifndef NODO_STORAGE_BLOCK_SNAPSHOT_HEADER_HPP
#define NODO_STORAGE_BLOCK_SNAPSHOT_HEADER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::storage {

/*
 * BlockSnapshotHeader is a validated metadata view extracted from a serialized
 * block snapshot on disk.
 *
 * Security principle:
 * Before full block deserialization, a node should verify the stored block
 * header, block hash, record count, and previous-hash continuity.
 */
class BlockSnapshotHeader {
public:
    static BlockSnapshotHeader fromSerializedBlock(
        const std::string& serializedBlock
    );

    static BlockSnapshotHeader fromFile(
        const std::string& filePath
    );

    static bool validateHeaderSequence(
        const std::vector<BlockSnapshotHeader>& headers
    );

    BlockSnapshotHeader(
        std::uint64_t blockIndex,
        std::string previousHash,
        std::string blockHash,
        std::int64_t timestamp,
        std::size_t recordCount,
        std::string headerPayload,
        std::string calculatedHash
    );

    std::uint64_t blockIndex() const;
    const std::string& previousHash() const;
    const std::string& blockHash() const;
    std::int64_t timestamp() const;
    std::size_t recordCount() const;
    const std::string& headerPayload() const;
    const std::string& calculatedHash() const;

    bool isGenesisHeader() const;
    bool isValid() const;

    std::string serialize() const;

private:
    static std::string extractHeaderPayload(
        const std::string& serializedBlock
    );

    static std::size_t countLedgerRecordsInHeaderPayload(
        const std::string& headerPayload
    );

    static std::string readFile(
        const std::string& filePath
    );

    static bool isSafeHash(
        const std::string& hash
    );

    static bool isSafePreviousHash(
        const std::string& previousHash
    );

    static std::string hashString(
        const std::string& value
    );

    std::uint64_t m_blockIndex;
    std::string m_previousHash;
    std::string m_blockHash;
    std::int64_t m_timestamp;
    std::size_t m_recordCount;
    std::string m_headerPayload;
    std::string m_calculatedHash;
};

} // namespace nodo::storage

#endif