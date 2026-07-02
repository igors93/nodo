#include "storage/BlockchainStorageReader.hpp"

#include "crypto/hash.h"
#include "serialization/FieldCodec.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

using nodo::serialization::FieldCodec;

StoredBlockSnapshot
StoredBlockSnapshot::fromIndexEntry(const std::string &rootDirectory,
                                    const BlockIndexEntry &entry) {
  if (rootDirectory.empty()) {
    throw std::invalid_argument("Storage root directory cannot be empty.");
  }

  if (!entry.isValid()) {
    throw std::invalid_argument(
        "Invalid BlockIndexEntry rejected by storage reader.");
  }

  const std::string filePath =
      buildSnapshotPath(rootDirectory, entry.fileName());

  const std::string content = readFile(filePath);

  if (!snapshotContentMatchesEntry(content, entry)) {
    throw std::logic_error(
        "Stored block snapshot content does not match BlockIndexEntry.");
  }

  StoredBlockSnapshot snapshot(entry.blockIndex(), entry.blockHash(),
                               entry.fileName(), filePath, content.size(),
                               hashString(content));

  if (!snapshot.isValid()) {
    throw std::logic_error("Generated StoredBlockSnapshot is invalid.");
  }

  return snapshot;
}

StoredBlockSnapshot::StoredBlockSnapshot(
    std::uint64_t blockIndex, std::string blockHash, std::string fileName,
    std::string filePath, std::size_t contentSize, std::string contentHash)
    : m_blockIndex(blockIndex), m_blockHash(std::move(blockHash)),
      m_fileName(std::move(fileName)), m_filePath(std::move(filePath)),
      m_contentSize(contentSize), m_contentHash(std::move(contentHash)) {}

std::uint64_t StoredBlockSnapshot::blockIndex() const { return m_blockIndex; }

const std::string &StoredBlockSnapshot::blockHash() const {
  return m_blockHash;
}

const std::string &StoredBlockSnapshot::fileName() const { return m_fileName; }

const std::string &StoredBlockSnapshot::filePath() const { return m_filePath; }

std::size_t StoredBlockSnapshot::contentSize() const { return m_contentSize; }

const std::string &StoredBlockSnapshot::contentHash() const {
  return m_contentHash;
}

bool StoredBlockSnapshot::isValid() const {
  if (!isSafeHash(m_blockHash)) {
    return false;
  }

  if (!isSafeFileName(m_fileName)) {
    return false;
  }

  if (m_filePath.empty()) {
    return false;
  }

  if (m_contentSize == 0) {
    return false;
  }

  if (!isSafeHash(m_contentHash)) {
    return false;
  }

  return true;
}

std::string StoredBlockSnapshot::serialize() const {
  std::ostringstream oss;

  oss << "StoredBlockSnapshot{"
      << "blockIndex=" << m_blockIndex << ";blockHash=" << m_blockHash
      << ";fileName=" << m_fileName << ";filePath=" << m_filePath
      << ";contentSize=" << m_contentSize << ";contentHash=" << m_contentHash
      << "}";

  return oss.str();
}

std::string
StoredBlockSnapshot::buildSnapshotPath(const std::string &rootDirectory,
                                       const std::string &fileName) {
  if (rootDirectory.empty()) {
    throw std::invalid_argument("Storage root directory cannot be empty.");
  }

  if (!isSafeFileName(fileName)) {
    throw std::invalid_argument("Unsafe block snapshot file name rejected.");
  }

  return (std::filesystem::path(rootDirectory) / "blocks" / fileName).string();
}

std::string StoredBlockSnapshot::readFile(const std::string &filePath) {
  if (filePath.empty()) {
    throw std::invalid_argument("Storage file path cannot be empty.");
  }

  std::ifstream input(filePath, std::ios::in | std::ios::binary);

  if (!input.is_open()) {
    throw std::runtime_error(
        "Failed to open stored block snapshot for reading.");
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  if (!input.good() && !input.eof()) {
    throw std::runtime_error("Failed while reading stored block snapshot.");
  }

  return buffer.str();
}

bool StoredBlockSnapshot::snapshotContentMatchesEntry(
    const std::string &content, const BlockIndexEntry &entry) {
  if (content.empty()) {
    return false;
  }

  if (content.rfind("Block{", 0) != 0) {
    return false;
  }

  try {
    const std::uint64_t serializedIndex = static_cast<std::uint64_t>(
        std::stoull(FieldCodec::extractField(content, "index")));

    const std::string serializedHash =
        FieldCodec::extractField(content, "hash");

    if (serializedIndex != entry.blockIndex()) {
      return false;
    }

    if (serializedHash != entry.blockHash()) {
      return false;
    }

    return true;
  } catch (...) {
    return false;
  }
}

bool StoredBlockSnapshot::isSafeHash(const std::string &hash) {
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

bool StoredBlockSnapshot::isSafeFileName(const std::string &fileName) {
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

std::string StoredBlockSnapshot::hashString(const std::string &value) {
  char output[65] = {0};
  nodo_hash_string(value.c_str(), output, sizeof(output));

  return std::string(output);
}

BlockchainStorageReadReport::BlockchainStorageReadReport()
    : m_success(true), m_failureReason(""), m_manifestValid(false),
      m_indexValid(false), m_indexMatchesManifest(false),
      m_manifestBlockCount(0), m_indexBlockCount(0), m_blockSnapshotCount(0),
      m_totalSnapshotBytes(0) {}

bool BlockchainStorageReadReport::success() const { return m_success; }

const std::string &BlockchainStorageReadReport::failureReason() const {
  return m_failureReason;
}

bool BlockchainStorageReadReport::manifestValid() const {
  return m_manifestValid;
}

bool BlockchainStorageReadReport::indexValid() const { return m_indexValid; }

bool BlockchainStorageReadReport::indexMatchesManifest() const {
  return m_indexMatchesManifest;
}

std::size_t BlockchainStorageReadReport::manifestBlockCount() const {
  return m_manifestBlockCount;
}

std::size_t BlockchainStorageReadReport::indexBlockCount() const {
  return m_indexBlockCount;
}

std::size_t BlockchainStorageReadReport::blockSnapshotCount() const {
  return m_blockSnapshotCount;
}

std::size_t BlockchainStorageReadReport::totalSnapshotBytes() const {
  return m_totalSnapshotBytes;
}

void BlockchainStorageReadReport::markFailure(std::string reason) {
  m_success = false;
  m_failureReason = std::move(reason);
}

void BlockchainStorageReadReport::setManifestValid(bool value) {
  m_manifestValid = value;
}

void BlockchainStorageReadReport::setIndexValid(bool value) {
  m_indexValid = value;
}

void BlockchainStorageReadReport::setIndexMatchesManifest(bool value) {
  m_indexMatchesManifest = value;
}

void BlockchainStorageReadReport::setManifestBlockCount(std::size_t value) {
  m_manifestBlockCount = value;
}

void BlockchainStorageReadReport::setIndexBlockCount(std::size_t value) {
  m_indexBlockCount = value;
}

void BlockchainStorageReadReport::setBlockSnapshotCount(std::size_t value) {
  m_blockSnapshotCount = value;
}

void BlockchainStorageReadReport::setTotalSnapshotBytes(std::size_t value) {
  m_totalSnapshotBytes = value;
}

std::string BlockchainStorageReadReport::serialize() const {
  std::ostringstream oss;

  oss << "BlockchainStorageReadReport{"
      << "success=" << (m_success ? "true" : "false")
      << ";failureReason=" << m_failureReason
      << ";manifestValid=" << (m_manifestValid ? "true" : "false")
      << ";indexValid=" << (m_indexValid ? "true" : "false")
      << ";indexMatchesManifest=" << (m_indexMatchesManifest ? "true" : "false")
      << ";manifestBlockCount=" << m_manifestBlockCount
      << ";indexBlockCount=" << m_indexBlockCount
      << ";blockSnapshotCount=" << m_blockSnapshotCount
      << ";totalSnapshotBytes=" << m_totalSnapshotBytes << "}";

  return oss.str();
}

BlockchainStorageReadReport
BlockchainStorageReader::auditStorageRoot(const std::string &rootDirectory) {
  BlockchainStorageReadReport report;

  try {
    ChainManifest manifest = ChainManifest::readFromStorageRoot(rootDirectory);

    const bool manifestValid = manifest.isValid();

    report.setManifestValid(manifestValid);
    report.setManifestBlockCount(manifest.blockCount());

    if (!manifestValid) {
      report.markFailure("Stored ChainManifest is invalid.");
      return report;
    }

    BlockStorageIndex index =
        BlockStorageIndex::readFromStorageRoot(rootDirectory);

    const bool indexValid = index.isValid();

    report.setIndexValid(indexValid);
    report.setIndexBlockCount(index.blockCount());

    if (!indexValid) {
      report.markFailure("Stored BlockStorageIndex is invalid.");
      return report;
    }

    validateManifestAndIndexCompatibility(manifest, index);
    report.setIndexMatchesManifest(true);

    const std::vector<StoredBlockSnapshot> snapshots =
        readBlockSnapshots(rootDirectory);

    std::size_t totalSnapshotBytes = 0;

    for (const auto &snapshot : snapshots) {
      if (!snapshot.isValid()) {
        report.markFailure("Invalid stored block snapshot found.");
        return report;
      }

      totalSnapshotBytes += snapshot.contentSize();
    }

    report.setBlockSnapshotCount(snapshots.size());
    report.setTotalSnapshotBytes(totalSnapshotBytes);

    if (snapshots.size() != manifest.blockCount()) {
      report.markFailure("Stored snapshot count does not match ChainManifest.");
      return report;
    }

    return report;
  } catch (const std::exception &error) {
    report.markFailure(error.what());
    return report;
  }
}

std::vector<StoredBlockSnapshot>
BlockchainStorageReader::readBlockSnapshots(const std::string &rootDirectory) {
  if (rootDirectory.empty()) {
    throw std::invalid_argument("Storage root directory cannot be empty.");
  }

  const ChainManifest manifest =
      ChainManifest::readFromStorageRoot(rootDirectory);

  if (!manifest.isValid()) {
    throw std::logic_error("Cannot read snapshots from invalid ChainManifest.");
  }

  const BlockStorageIndex index =
      BlockStorageIndex::readFromStorageRoot(rootDirectory);

  if (!index.isValid()) {
    throw std::logic_error(
        "Cannot read snapshots from invalid BlockStorageIndex.");
  }

  validateManifestAndIndexCompatibility(manifest, index);

  std::vector<StoredBlockSnapshot> snapshots;

  for (const auto &entry : index.entries()) {
    snapshots.push_back(
        StoredBlockSnapshot::fromIndexEntry(rootDirectory, entry));
  }

  return snapshots;
}

void BlockchainStorageReader::validateManifestAndIndexCompatibility(
    const ChainManifest &manifest, const BlockStorageIndex &index) {
  if (!manifest.isValid()) {
    throw std::logic_error("Invalid ChainManifest rejected by storage reader.");
  }

  if (!index.isValid()) {
    throw std::logic_error(
        "Invalid BlockStorageIndex rejected by storage reader.");
  }

  if (index.chainManifestHash() != manifest.manifestHash()) {
    throw std::logic_error(
        "BlockStorageIndex does not reference ChainManifest.");
  }

  if (index.blockCount() != manifest.blockCount()) {
    throw std::logic_error(
        "BlockStorageIndex block count does not match ChainManifest.");
  }
}

} // namespace nodo::storage
