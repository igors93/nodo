#ifndef NODO_STORAGE_BLOCK_FILE_STORE_HPP
#define NODO_STORAGE_BLOCK_FILE_STORE_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace nodo::storage {

/*
 * BlockFileStore persists deterministic block snapshots to disk.
 *
 * Security principle:
 * Storage must not silently rewrite ledger history.
 *
 * Current status:
 * This is a storage foundation. It stores validated block snapshots, but it
 * does not yet deserialize blocks back into a Blockchain.
 */
class BlockFileStore {
public:
    explicit BlockFileStore(std::string rootDirectory);

    const std::string& rootDirectory() const;
    std::string blockDirectoryPath() const;

    void ensureStorageDirectory() const;

    /*
     * Deletes stored block snapshots and recreates the block directory.
     *
     * Development note:
     * This is useful for local demo runs. Production nodes must never erase
     * chain storage without an explicit operator decision.
     */
    void clearBlockStorage() const;

    /*
     * Stores a single valid block.
     *
     * If the same file already exists with identical content, the operation is
     * treated as idempotent. If the file exists with different content, the
     * write is rejected.
     */
    void writeBlock(const core::Block& block) const;

    /*
     * Stores all blocks from a valid Blockchain.
     */
    void writeBlockchain(const core::Blockchain& blockchain) const;

    bool hasStoredBlock(const core::Block& block) const;
    bool verifyStoredBlock(const core::Block& block) const;

    std::string readBlockSnapshot(const core::Block& block) const;

    std::size_t storedBlockFileCount() const;

    std::string serialize() const;

private:
    std::string m_rootDirectory;

    std::filesystem::path rootPath() const;
    std::filesystem::path blockDirectory() const;
    std::filesystem::path blockFilePath(const core::Block& block) const;

    static std::string blockFileName(const core::Block& block);
    static void validateHashForFileName(const std::string& hash);
};

} // namespace nodo::storage

#endif