#ifndef NODO_STORAGE_CHAIN_MANIFEST_HPP
#define NODO_STORAGE_CHAIN_MANIFEST_HPP

#include "core/Blockchain.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::storage {

/*
 * ChainManifest summarizes the blockchain persisted on disk.
 *
 * Security principle:
 * Before a node loads detailed block data from storage, it should have a
 * small deterministic summary describing which chain is expected.
 *
 * Current status:
 * This manifest validates saved chain metadata. Full block deserialization
 * and chain loading will be implemented in a later phase.
 */
class ChainManifest {
public:
    static ChainManifest fromBlockchain(
        const core::Blockchain& blockchain,
        std::int64_t createdAt
    );

    static ChainManifest deserialize(
        const std::string& serialized
    );

    static std::string manifestFileName();

    static std::string manifestFilePath(
        const std::string& rootDirectory
    );

    static ChainManifest readFromStorageRoot(
        const std::string& rootDirectory
    );

    ChainManifest(
        std::string chainVersion,
        std::size_t blockCount,
        std::string genesisHash,
        std::string latestHash,
        std::int64_t createdAt,
        std::string manifestHash
    );

    const std::string& chainVersion() const;
    std::size_t blockCount() const;
    const std::string& genesisHash() const;
    const std::string& latestHash() const;
    std::int64_t createdAt() const;
    const std::string& manifestHash() const;

    bool isValid() const;
    bool matchesBlockchain(const core::Blockchain& blockchain) const;

    void writeToStorageRoot(
        const std::string& rootDirectory
    ) const;

    std::string serialize() const;

private:
    static std::string currentManifestVersion();

    static std::string computeManifestHash(
        const std::string& chainVersion,
        std::size_t blockCount,
        const std::string& genesisHash,
        const std::string& latestHash,
        std::int64_t createdAt
    );

    static bool isSafeHash(
        const std::string& hash
    );

    static std::string hashString(
        const std::string& value
    );

    std::string m_chainVersion;
    std::size_t m_blockCount;
    std::string m_genesisHash;
    std::string m_latestHash;
    std::int64_t m_createdAt;
    std::string m_manifestHash;
};

} // namespace nodo::storage

#endif
