#include "storage/BlockStorageIndex.hpp"

#include "crypto/hash.h"
#include "serialization/BlockStorageIndexCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

BlockIndexEntry BlockIndexEntry::fromBlock(const core::Block &block) {
  if (!block.isValid(false)) {
    throw std::invalid_argument("Invalid block rejected by BlockIndexEntry.");
  }

  return BlockIndexEntry(block.index(), block.hash(),
                         expectedFileName(block.index(), block.hash()));
}

BlockIndexEntry BlockIndexEntry::deserialize(const std::string &serialized) {
  return serialization::BlockStorageIndexCodec::deserializeEntry(serialized);
}

BlockIndexEntry::BlockIndexEntry(std::uint64_t blockIndex,
                                 std::string blockHash, std::string fileName)
    : m_blockIndex(blockIndex), m_blockHash(std::move(blockHash)),
      m_fileName(std::move(fileName)) {}

std::uint64_t BlockIndexEntry::blockIndex() const { return m_blockIndex; }

const std::string &BlockIndexEntry::blockHash() const { return m_blockHash; }

const std::string &BlockIndexEntry::fileName() const { return m_fileName; }

bool BlockIndexEntry::isValid() const {
  if (!isSafeHash(m_blockHash)) {
    return false;
  }

  if (!isSafeFileName(m_fileName)) {
    return false;
  }

  if (m_fileName != expectedFileName(m_blockIndex, m_blockHash)) {
    return false;
  }

  return true;
}

bool BlockIndexEntry::matchesBlock(const core::Block &block) const {
  if (!isValid()) {
    return false;
  }

  if (!block.isValid(false)) {
    return false;
  }

  if (m_blockIndex != block.index()) {
    return false;
  }

  if (m_blockHash != block.hash()) {
    return false;
  }

  if (m_fileName != expectedFileName(block.index(), block.hash())) {
    return false;
  }

  return true;
}

std::string BlockIndexEntry::serialize() const {
  std::ostringstream oss;

  oss << "BlockIndexEntry{"
      << "blockIndex=" << m_blockIndex << ";blockHash=" << m_blockHash
      << ";fileName=" << m_fileName << "}";

  return oss.str();
}

std::string BlockIndexEntry::expectedFileName(std::uint64_t blockIndex,
                                              const std::string &blockHash) {
  if (!isSafeHash(blockHash)) {
    throw std::invalid_argument(
        "Unsafe block hash rejected by BlockIndexEntry.");
  }

  return "block_" + std::to_string(blockIndex) + "_" + blockHash + ".nodo";
}

bool BlockIndexEntry::isSafeHash(const std::string &hash) {
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

bool BlockIndexEntry::isSafeFileName(const std::string &fileName) {
  if (fileName.empty()) {
    return false;
  }

  if (fileName.find('/') != std::string::npos ||
      fileName.find('\\') != std::string::npos ||
      fileName.find("..") != std::string::npos) {
    return false;
  }

  for (const char current : fileName) {
    const bool isDigit = current >= '0' && current <= '9';
    const bool isLower = current >= 'a' && current <= 'z';
    const bool isUpper = current >= 'A' && current <= 'Z';
    const bool isAllowedSymbol =
        current == '_' || current == '-' || current == '.';

    if (!isDigit && !isLower && !isUpper && !isAllowedSymbol) {
      return false;
    }
  }

  return true;
}

BlockStorageIndex
BlockStorageIndex::fromBlockchainAndManifest(const core::Blockchain &blockchain,
                                             const ChainManifest &manifest,
                                             std::int64_t createdAt) {
  if (blockchain.empty()) {
    throw std::invalid_argument(
        "Empty Blockchain rejected by BlockStorageIndex.");
  }

  if (!blockchain.isValid(false)) {
    throw std::invalid_argument(
        "Invalid Blockchain rejected by BlockStorageIndex.");
  }

  if (!manifest.matchesBlockchain(blockchain)) {
    throw std::invalid_argument("ChainManifest does not match Blockchain.");
  }

  if (createdAt <= 0) {
    throw std::invalid_argument(
        "BlockStorageIndex timestamp must be positive.");
  }

  std::vector<BlockIndexEntry> entries;

  for (const auto &block : blockchain.blocks()) {
    entries.push_back(BlockIndexEntry::fromBlock(block));
  }

  const std::string version = currentIndexVersion();
  const std::string chainManifestHash = manifest.manifestHash();
  const std::size_t blockCount = blockchain.size();

  const std::string indexHash = computeIndexHash(
      version, chainManifestHash, blockCount, entries, createdAt);

  BlockStorageIndex index(version, chainManifestHash, blockCount,
                          std::move(entries), createdAt, indexHash);

  if (!index.isValid()) {
    throw std::logic_error("Generated BlockStorageIndex is invalid.");
  }

  return index;
}

BlockStorageIndex
BlockStorageIndex::deserialize(const std::string &serialized) {
  return serialization::BlockStorageIndexCodec::deserialize(serialized);
}

std::string BlockStorageIndex::indexFileName() { return "block_index.nodo"; }

std::string BlockStorageIndex::indexFilePath(const std::string &rootDirectory) {
  if (rootDirectory.empty()) {
    throw std::invalid_argument(
        "BlockStorageIndex root directory cannot be empty.");
  }

  return (std::filesystem::path(rootDirectory) / indexFileName()).string();
}

BlockStorageIndex
BlockStorageIndex::readFromStorageRoot(const std::string &rootDirectory) {
  const std::filesystem::path path = indexFilePath(rootDirectory);

  std::ifstream input(path, std::ios::in | std::ios::binary);

  if (!input.is_open()) {
    throw std::runtime_error("Failed to open BlockStorageIndex for reading.");
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  if (!input.good() && !input.eof()) {
    throw std::runtime_error("Failed while reading BlockStorageIndex.");
  }

  return deserialize(buffer.str());
}

BlockStorageIndex::BlockStorageIndex(std::string indexVersion,
                                     std::string chainManifestHash,
                                     std::size_t blockCount,
                                     std::vector<BlockIndexEntry> entries,
                                     std::int64_t createdAt,
                                     std::string indexHash)
    : m_indexVersion(std::move(indexVersion)),
      m_chainManifestHash(std::move(chainManifestHash)),
      m_blockCount(blockCount), m_entries(std::move(entries)),
      m_createdAt(createdAt), m_indexHash(std::move(indexHash)) {}

const std::string &BlockStorageIndex::indexVersion() const {
  return m_indexVersion;
}

const std::string &BlockStorageIndex::chainManifestHash() const {
  return m_chainManifestHash;
}

std::size_t BlockStorageIndex::blockCount() const { return m_blockCount; }

const std::vector<BlockIndexEntry> &BlockStorageIndex::entries() const {
  return m_entries;
}

std::int64_t BlockStorageIndex::createdAt() const { return m_createdAt; }

const std::string &BlockStorageIndex::indexHash() const { return m_indexHash; }

/**
 * Validates the metadata structure of the block storage index.
 * Ensures the index correctly maps block heights to sequential files without
 * duplicates, matches the required format, and verifies the overall index
 * checksum.
 */
bool BlockStorageIndex::isValid() const {
  if (m_indexVersion != currentIndexVersion()) {
    return false;
  }

  if (!isSafeHash(m_chainManifestHash)) {
    return false;
  }

  if (m_blockCount == 0) {
    return false;
  }

  if (m_entries.size() != m_blockCount) {
    return false;
  }

  if (m_createdAt <= 0) {
    return false;
  }

  if (!isSafeHash(m_indexHash)) {
    return false;
  }

  if (!hasStrictlySequentialEntries(m_entries)) {
    return false;
  }

  if (hasDuplicateHashes(m_entries)) {
    return false;
  }

  if (hasDuplicateFileNames(m_entries)) {
    return false;
  }

  for (const auto &entry : m_entries) {
    if (!entry.isValid()) {
      return false;
    }
  }

  const std::string expectedHash =
      computeIndexHash(m_indexVersion, m_chainManifestHash, m_blockCount,
                       m_entries, m_createdAt);

  if (m_indexHash != expectedHash) {
    return false;
  }

  return true;
}

bool BlockStorageIndex::matchesBlockchainAndManifest(
    const core::Blockchain &blockchain, const ChainManifest &manifest) const {
  if (!isValid()) {
    return false;
  }

  if (!manifest.matchesBlockchain(blockchain)) {
    return false;
  }

  if (m_chainManifestHash != manifest.manifestHash()) {
    return false;
  }

  if (m_blockCount != blockchain.size()) {
    return false;
  }

  if (m_entries.size() != blockchain.blocks().size()) {
    return false;
  }

  for (std::size_t i = 0; i < m_entries.size(); ++i) {
    if (!m_entries[i].matchesBlock(blockchain.blocks()[i])) {
      return false;
    }
  }

  return true;
}

void BlockStorageIndex::writeToStorageRoot(
    const std::string &rootDirectory) const {
  if (!isValid()) {
    throw std::invalid_argument(
        "Invalid BlockStorageIndex rejected by storage writer.");
  }

  if (rootDirectory.empty()) {
    throw std::invalid_argument(
        "BlockStorageIndex root directory cannot be empty.");
  }

  const std::filesystem::path rootPath(rootDirectory);
  std::error_code errorCode;

  std::filesystem::create_directories(rootPath, errorCode);

  if (errorCode) {
    throw std::runtime_error(
        "Failed to create BlockStorageIndex storage directory: " +
        errorCode.message());
  }

  const std::filesystem::path finalPath = indexFilePath(rootDirectory);
  AtomicFile::writeTextFile(finalPath, serialize());

  const BlockStorageIndex storedIndex = readFromStorageRoot(rootDirectory);

  if (storedIndex.serialize() != serialize()) {
    throw std::runtime_error("Stored BlockStorageIndex verification failed.");
  }
}

std::string BlockStorageIndex::serialize() const {
  std::ostringstream oss;

  oss << "BlockStorageIndex{"
      << "indexVersion=" << m_indexVersion
      << ";chainManifestHash=" << m_chainManifestHash
      << ";blockCount=" << m_blockCount << ";createdAt=" << m_createdAt
      << ";indexHash=" << m_indexHash << ";entries=[";

  for (std::size_t i = 0; i < m_entries.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }

    oss << m_entries[i].serialize();
  }

  oss << "]}";

  return oss.str();
}

std::string BlockStorageIndex::currentIndexVersion() {
  return "NODO_BLOCK_STORAGE_INDEX_V1";
}

std::string BlockStorageIndex::computeIndexHash(
    const std::string &indexVersion, const std::string &chainManifestHash,
    std::size_t blockCount, const std::vector<BlockIndexEntry> &entries,
    std::int64_t createdAt) {
  std::ostringstream oss;

  /*
   * Deterministic index hash.
   *
   * This checksum protects the metadata file itself. It does not replace
   * block hash validation.
   */
  oss << "BlockStorageIndexPayload{"
      << "indexVersion=" << indexVersion
      << ";chainManifestHash=" << chainManifestHash
      << ";blockCount=" << blockCount << ";createdAt=" << createdAt
      << ";entries=[";

  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }

    oss << entries[i].serialize();
  }

  oss << "]}";

  return hashString(oss.str());
}

bool BlockStorageIndex::hasStrictlySequentialEntries(
    const std::vector<BlockIndexEntry> &entries) {
  if (entries.empty()) {
    return false;
  }

  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].blockIndex() != i) {
      return false;
    }
  }

  return true;
}

bool BlockStorageIndex::hasDuplicateHashes(
    const std::vector<BlockIndexEntry> &entries) {
  for (std::size_t i = 0; i < entries.size(); ++i) {
    for (std::size_t j = i + 1; j < entries.size(); ++j) {
      if (entries[i].blockHash() == entries[j].blockHash()) {
        return true;
      }
    }
  }

  return false;
}

bool BlockStorageIndex::hasDuplicateFileNames(
    const std::vector<BlockIndexEntry> &entries) {
  for (std::size_t i = 0; i < entries.size(); ++i) {
    for (std::size_t j = i + 1; j < entries.size(); ++j) {
      if (entries[i].fileName() == entries[j].fileName()) {
        return true;
      }
    }
  }

  return false;
}

bool BlockStorageIndex::isSafeHash(const std::string &hash) {
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

std::string BlockStorageIndex::hashString(const std::string &value) {
  char output[65] = {0};
  nodo_hash_string(value.c_str(), output, sizeof(output));

  return std::string(output);
}

} // namespace nodo::storage
