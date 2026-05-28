#ifndef NODO_STORAGE_BLOCK_STORAGE_INDEX_HPP
#define NODO_STORAGE_BLOCK_STORAGE_INDEX_HPP

#include "core/Blockchain.hpp"
#include "storage/ChainManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::storage {

/*
 * BlockIndexEntry maps a block height and block hash to the deterministic
 * storage file expected on disk.
 *
 * Security principle:
 * A persisted block should be located through deterministic metadata, not
 * through directory guessing or unsafe file names.
 */
class BlockIndexEntry {
public:
    static BlockIndexEntry fromBlock(
        const core::Block& block
    );

    static BlockIndexEntry deserialize(
        const std::string& serialized
    );

    BlockIndexEntry(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string fileName
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& fileName() const;

    bool isValid() const;
    bool matchesBlock(const core::Block& block) const;

    std::string serialize() const;

private:
    static std::string expectedFileName(
        std::uint64_t blockIndex,
        const std::string& blockHash
    );

    static bool isSafeHash(
        const std::string& hash
    );

    static bool isSafeFileName(
        const std::string& fileName
    );

    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_fileName;
};

/*
 * BlockStorageIndex describes the expected block snapshot files for a stored
 * chain.
 *
 * Security principle:
 * Storage loading should know exactly which files belong to the chain before
 * attempting to rebuild state from disk.
 *
 * Current status:
 * This index is a metadata foundation. It does not deserialize blocks yet.
 */
class BlockStorageIndex {
public:
    static BlockStorageIndex fromBlockchainAndManifest(
        const core::Blockchain& blockchain,
        const ChainManifest& manifest,
        std::int64_t createdAt
    );

    static BlockStorageIndex deserialize(
        const std::string& serialized
    );

    static std::string indexFileName();

    static std::string indexFilePath(
        const std::string& rootDirectory
    );

    static BlockStorageIndex readFromStorageRoot(
        const std::string& rootDirectory
    );

    BlockStorageIndex(
        std::string indexVersion,
        std::string chainManifestHash,
        std::size_t blockCount,
        std::vector<BlockIndexEntry> entries,
        std::int64_t createdAt,
        std::string indexHash
    );

    const std::string& indexVersion() const;
    const std::string& chainManifestHash() const;
    std::size_t blockCount() const;
    const std::vector<BlockIndexEntry>& entries() const;
    std::int64_t createdAt() const;
    const std::string& indexHash() const;

    bool isValid() const;

    bool matchesBlockchainAndManifest(
        const core::Blockchain& blockchain,
        const ChainManifest& manifest
    ) const;

    void writeToStorageRoot(
        const std::string& rootDirectory
    ) const;

    std::string serialize() const;

private:
    static std::string currentIndexVersion();

    static std::string computeIndexHash(
        const std::string& indexVersion,
        const std::string& chainManifestHash,
        std::size_t blockCount,
        const std::vector<BlockIndexEntry>& entries,
        std::int64_t createdAt
    );

    static bool hasStrictlySequentialEntries(
        const std::vector<BlockIndexEntry>& entries
    );

    static bool hasDuplicateHashes(
        const std::vector<BlockIndexEntry>& entries
    );

    static bool hasDuplicateFileNames(
        const std::vector<BlockIndexEntry>& entries
    );

    static bool isSafeHash(
        const std::string& hash
    );

    static std::string hashString(
        const std::string& value
    );

    std::string m_indexVersion;
    std::string m_chainManifestHash;
    std::size_t m_blockCount;
    std::vector<BlockIndexEntry> m_entries;
    std::int64_t m_createdAt;
    std::string m_indexHash;
};

} // namespace nodo::storage

#endif
