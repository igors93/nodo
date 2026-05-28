#ifndef NODO_STORAGE_BLOCKCHAIN_LOADER_HPP
#define NODO_STORAGE_BLOCKCHAIN_LOADER_HPP

#include "core/Blockchain.hpp"

#include <cstddef>
#include <string>

namespace nodo::storage {

/*
 * BlockchainLoadReport summarizes the result of rebuilding a Blockchain from
 * persisted storage.
 *
 * Security principle:
 * Loading from disk must be auditable. A failed load should explain where the
 * storage validation failed.
 */
class BlockchainLoadReport {
public:
    BlockchainLoadReport();

    bool success() const;
    const std::string& failureReason() const;

    bool storageReaderValid() const;
    bool loadedBlockchainValid() const;
    bool loadedBlockchainMatchesManifest() const;
    bool loadedBlockchainMatchesIndex() const;

    std::size_t loadedBlockCount() const;

    void markFailure(std::string reason);

    void setStorageReaderValid(bool value);
    void setLoadedBlockchainValid(bool value);
    void setLoadedBlockchainMatchesManifest(bool value);
    void setLoadedBlockchainMatchesIndex(bool value);
    void setLoadedBlockCount(std::size_t value);

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    bool m_storageReaderValid;
    bool m_loadedBlockchainValid;
    bool m_loadedBlockchainMatchesManifest;
    bool m_loadedBlockchainMatchesIndex;

    std::size_t m_loadedBlockCount;
};

/*
 * BlockchainLoader rebuilds a Blockchain object from Nodo storage metadata and
 * block snapshot files.
 *
 * Expected storage layout:
 *
 * data/chain_manifest.nodo
 * data/block_index.nodo
 * data/blocks/block_<height>_<hash>.nodo
 *
 * Current status:
 * This is the first complete disk-to-Blockchain loader foundation. Future
 * phases should add stronger storage tests, canonical binary serialization,
 * and production-grade cryptographic hashing.
 */
class BlockchainLoader {
public:
    static BlockchainLoadReport auditStorageRoot(
        const std::string& rootDirectory
    );

    static core::Blockchain loadFromStorageRoot(
        const std::string& rootDirectory
    );

private:
    static core::Blockchain buildBlockchainFromBlocks(
        const std::vector<core::Block>& blocks
    );
};

} // namespace nodo::storage

#endif