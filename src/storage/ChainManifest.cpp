#include "storage/ChainManifest.hpp"

#include "crypto/hash.h"
#include "serialization/ChainManifestCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

ChainManifest ChainManifest::fromBlockchain(
    const core::Blockchain& blockchain,
    std::int64_t createdAt
) {
    if (blockchain.empty()) {
        throw std::invalid_argument("Empty Blockchain rejected by ChainManifest.");
    }

    if (!blockchain.isValid(false)) {
        throw std::invalid_argument("Invalid Blockchain rejected by ChainManifest.");
    }

    if (createdAt <= 0) {
        throw std::invalid_argument("ChainManifest timestamp must be positive.");
    }

    const std::string version = currentManifestVersion();
    const std::size_t blockCount = blockchain.size();
    const std::string genesisHash = blockchain.genesisBlock().hash();
    const std::string latestHash = blockchain.latestBlock().hash();

    const std::string manifestHash = computeManifestHash(
        version,
        blockCount,
        genesisHash,
        latestHash,
        createdAt
    );

    ChainManifest manifest(
        version,
        blockCount,
        genesisHash,
        latestHash,
        createdAt,
        manifestHash
    );

    if (!manifest.isValid()) {
        throw std::logic_error("Generated ChainManifest is invalid.");
    }

    return manifest;
}

ChainManifest ChainManifest::deserialize(
    const std::string& serialized
) {
    return serialization::ChainManifestCodec::deserialize(serialized);
}

std::string ChainManifest::manifestFileName() {
    return "chain_manifest.nodo";
}

std::string ChainManifest::manifestFilePath(
    const std::string& rootDirectory
) {
    if (rootDirectory.empty()) {
        throw std::invalid_argument("ChainManifest root directory cannot be empty.");
    }

    return (std::filesystem::path(rootDirectory) / manifestFileName()).string();
}

ChainManifest ChainManifest::readFromStorageRoot(
    const std::string& rootDirectory
) {
    const std::filesystem::path path = manifestFilePath(rootDirectory);

    std::ifstream input(path, std::ios::in | std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Failed to open ChainManifest for reading.");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed while reading ChainManifest.");
    }

    return deserialize(buffer.str());
}

ChainManifest::ChainManifest(
    std::string chainVersion,
    std::size_t blockCount,
    std::string genesisHash,
    std::string latestHash,
    std::int64_t createdAt,
    std::string manifestHash
)
    : m_chainVersion(std::move(chainVersion)),
      m_blockCount(blockCount),
      m_genesisHash(std::move(genesisHash)),
      m_latestHash(std::move(latestHash)),
      m_createdAt(createdAt),
      m_manifestHash(std::move(manifestHash)) {}

const std::string& ChainManifest::chainVersion() const {
    return m_chainVersion;
}

std::size_t ChainManifest::blockCount() const {
    return m_blockCount;
}

const std::string& ChainManifest::genesisHash() const {
    return m_genesisHash;
}

const std::string& ChainManifest::latestHash() const {
    return m_latestHash;
}

std::int64_t ChainManifest::createdAt() const {
    return m_createdAt;
}

const std::string& ChainManifest::manifestHash() const {
    return m_manifestHash;
}

bool ChainManifest::isValid() const {
    if (m_chainVersion != currentManifestVersion()) {
        return false;
    }

    if (m_blockCount == 0) {
        return false;
    }

    if (!isSafeHash(m_genesisHash)) {
        return false;
    }

    if (!isSafeHash(m_latestHash)) {
        return false;
    }

    if (m_createdAt <= 0) {
        return false;
    }

    if (!isSafeHash(m_manifestHash)) {
        return false;
    }

    const std::string expectedHash = computeManifestHash(
        m_chainVersion,
        m_blockCount,
        m_genesisHash,
        m_latestHash,
        m_createdAt
    );

    if (m_manifestHash != expectedHash) {
        return false;
    }

    return true;
}

bool ChainManifest::matchesBlockchain(
    const core::Blockchain& blockchain
) const {
    if (!isValid()) {
        return false;
    }

    if (blockchain.empty()) {
        return false;
    }

    if (!blockchain.isValid(false)) {
        return false;
    }

    if (m_blockCount != blockchain.size()) {
        return false;
    }

    if (m_genesisHash != blockchain.genesisBlock().hash()) {
        return false;
    }

    if (m_latestHash != blockchain.latestBlock().hash()) {
        return false;
    }

    return true;
}

void ChainManifest::writeToStorageRoot(
    const std::string& rootDirectory
) const {
    if (!isValid()) {
        throw std::invalid_argument("Invalid ChainManifest rejected by storage writer.");
    }

    if (rootDirectory.empty()) {
        throw std::invalid_argument("ChainManifest root directory cannot be empty.");
    }

    const std::filesystem::path rootPath(rootDirectory);
    std::error_code errorCode;

    std::filesystem::create_directories(rootPath, errorCode);

    if (errorCode) {
        throw std::runtime_error(
            "Failed to create ChainManifest storage directory: " +
            errorCode.message()
        );
    }

    const std::filesystem::path finalPath = manifestFilePath(rootDirectory);
    AtomicFile::writeTextFile(finalPath, serialize());

    const ChainManifest storedManifest = readFromStorageRoot(rootDirectory);

    if (storedManifest.serialize() != serialize()) {
        throw std::runtime_error("Stored ChainManifest verification failed.");
    }
}

std::string ChainManifest::serialize() const {
    std::ostringstream oss;

    oss << "ChainManifest{"
        << "chainVersion=" << m_chainVersion
        << ";blockCount=" << m_blockCount
        << ";genesisHash=" << m_genesisHash
        << ";latestHash=" << m_latestHash
        << ";createdAt=" << m_createdAt
        << ";manifestHash=" << m_manifestHash
        << "}";

    return oss.str();
}

std::string ChainManifest::currentManifestVersion() {
    return "NODO_CHAIN_MANIFEST_V1";
}

std::string ChainManifest::computeManifestHash(
    const std::string& chainVersion,
    std::size_t blockCount,
    const std::string& genesisHash,
    const std::string& latestHash,
    std::int64_t createdAt
) {
    std::ostringstream oss;

    /*
     * Deterministic manifest hash.
     *
     * This does not replace block validation. It gives storage a compact
     * metadata checksum for the persisted chain summary.
     */
    oss << "ChainManifestPayload{"
        << "chainVersion=" << chainVersion
        << ";blockCount=" << blockCount
        << ";genesisHash=" << genesisHash
        << ";latestHash=" << latestHash
        << ";createdAt=" << createdAt
        << "}";

    return hashString(oss.str());
}

bool ChainManifest::isSafeHash(
    const std::string& hash
) {
    if (hash.empty()) {
        return false;
    }

    for (const char current : hash) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';
        const bool isUpperHex = current >= 'A' && current <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            return false;
        }
    }

    return true;
}

std::string ChainManifest::hashString(
    const std::string& value
) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::storage
