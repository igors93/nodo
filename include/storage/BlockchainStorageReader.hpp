#ifndef NODO_STORAGE_BLOCKCHAIN_STORAGE_READER_HPP
#define NODO_STORAGE_BLOCKCHAIN_STORAGE_READER_HPP

#include "storage/BlockStorageIndex.hpp"
#include "storage/ChainManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::storage {

/*
 * StoredBlockSnapshot is a lightweight representation of a block file already
 * persisted on disk.
 *
 * Security principle:
 * Storage reading must validate metadata before future code attempts to
 * rebuild chain state from disk content.
 */
class StoredBlockSnapshot {
public:
    static StoredBlockSnapshot fromIndexEntry(
        const std::string& rootDirectory,
        const BlockIndexEntry& entry
    );

    StoredBlockSnapshot(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string fileName,
        std::string filePath,
        std::size_t contentSize,
        std::string contentHash
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& fileName() const;
    const std::string& filePath() const;
    std::size_t contentSize() const;
    const std::string& contentHash() const;

    bool isValid() const;

    std::string serialize() const;

private:
    static std::string buildSnapshotPath(
        const std::string& rootDirectory,
        const std::string& fileName
    );

    static std::string readFile(
        const std::string& filePath
    );

    static bool snapshotContentMatchesEntry(
        const std::string& content,
        const BlockIndexEntry& entry
    );

    static bool isSafeHash(
        const std::string& hash
    );

    static bool isSafeFileName(
        const std::string& fileName
    );

    static std::string hashString(
        const std::string& value
    );

    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_fileName;
    std::string m_filePath;
    std::size_t m_contentSize;
    std::string m_contentHash;
};

/*
 * BlockchainStorageReadReport summarizes the result of reading storage
 * metadata and block snapshots from disk.
 *
 * This is not full Blockchain deserialization yet.
 */
class BlockchainStorageReadReport {
public:
    BlockchainStorageReadReport();

    bool success() const;
    const std::string& failureReason() const;

    bool manifestValid() const;
    bool indexValid() const;
    bool indexMatchesManifest() const;

    std::size_t manifestBlockCount() const;
    std::size_t indexBlockCount() const;
    std::size_t blockSnapshotCount() const;
    std::size_t totalSnapshotBytes() const;

    void markFailure(std::string reason);

    void setManifestValid(bool value);
    void setIndexValid(bool value);
    void setIndexMatchesManifest(bool value);

    void setManifestBlockCount(std::size_t value);
    void setIndexBlockCount(std::size_t value);
    void setBlockSnapshotCount(std::size_t value);
    void setTotalSnapshotBytes(std::size_t value);

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    bool m_manifestValid;
    bool m_indexValid;
    bool m_indexMatchesManifest;

    std::size_t m_manifestBlockCount;
    std::size_t m_indexBlockCount;
    std::size_t m_blockSnapshotCount;
    std::size_t m_totalSnapshotBytes;
};

/*
 * BlockchainStorageReader validates the storage metadata path:
 *
 * data/chain_manifest.nodo
 * data/block_index.nodo
 * data/blocks/block_<height>_<hash>.nodo
 *
 * Current status:
 * This reader verifies that persisted block snapshots exist and match their
 * metadata. Full Block and Blockchain reconstruction will come later.
 */
class BlockchainStorageReader {
public:
    static BlockchainStorageReadReport auditStorageRoot(
        const std::string& rootDirectory
    );

    static std::vector<StoredBlockSnapshot> readBlockSnapshots(
        const std::string& rootDirectory
    );

private:
    static void validateManifestAndIndexCompatibility(
        const ChainManifest& manifest,
        const BlockStorageIndex& index
    );
};

} // namespace nodo::storage

#endif
