#include "storage/BlockSnapshotHeader.hpp"

#include "serialization/BlockSnapshotHeaderCodec.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

BlockSnapshotHeader
BlockSnapshotHeader::fromSerializedBlock(const std::string &serializedBlock) {
  return serialization::BlockSnapshotHeaderCodec::
      deserializeFromSerializedBlock(serializedBlock);
}

BlockSnapshotHeader BlockSnapshotHeader::fromFile(const std::string &filePath) {
  return fromSerializedBlock(readFile(filePath));
}

bool BlockSnapshotHeader::validateHeaderSequence(
    const std::vector<BlockSnapshotHeader> &headers) {
  if (headers.empty()) {
    return false;
  }

  for (std::size_t i = 0; i < headers.size(); ++i) {
    if (!headers[i].isValid()) {
      return false;
    }

    if (headers[i].blockIndex() != i) {
      return false;
    }

    if (i == 0) {
      if (!headers[i].isGenesisHeader()) {
        return false;
      }

      continue;
    }

    if (headers[i].previousHash() != headers[i - 1].blockHash()) {
      return false;
    }
  }

  return true;
}

BlockSnapshotHeader::BlockSnapshotHeader(
    std::uint64_t blockIndex, std::string previousHash, std::string blockHash,
    std::int64_t timestamp, std::size_t recordCount, std::string headerPayload,
    std::string calculatedHash)
    : m_blockIndex(blockIndex), m_previousHash(std::move(previousHash)),
      m_blockHash(std::move(blockHash)), m_timestamp(timestamp),
      m_recordCount(recordCount), m_headerPayload(std::move(headerPayload)),
      m_calculatedHash(std::move(calculatedHash)) {}

std::uint64_t BlockSnapshotHeader::blockIndex() const { return m_blockIndex; }

const std::string &BlockSnapshotHeader::previousHash() const {
  return m_previousHash;
}

const std::string &BlockSnapshotHeader::blockHash() const {
  return m_blockHash;
}

std::int64_t BlockSnapshotHeader::timestamp() const { return m_timestamp; }

std::size_t BlockSnapshotHeader::recordCount() const { return m_recordCount; }

const std::string &BlockSnapshotHeader::headerPayload() const {
  return m_headerPayload;
}

const std::string &BlockSnapshotHeader::calculatedHash() const {
  return m_calculatedHash;
}

bool BlockSnapshotHeader::isGenesisHeader() const {
  return m_blockIndex == 0 && m_previousHash == "GENESIS";
}

bool BlockSnapshotHeader::isValid() const {
  if (!isSafePreviousHash(m_previousHash)) {
    return false;
  }

  if (!isSafeHash(m_blockHash)) {
    return false;
  }

  if (m_timestamp <= 0) {
    return false;
  }

  if (m_recordCount == 0) {
    return false;
  }

  if (m_headerPayload.empty()) {
    return false;
  }

  if (m_headerPayload.rfind("BlockHeader{", 0) != 0) {
    return false;
  }

  if (!isSafeHash(m_calculatedHash)) {
    return false;
  }

  if (m_blockHash != m_calculatedHash) {
    return false;
  }

  if (m_blockIndex == 0 && m_previousHash != "GENESIS") {
    return false;
  }

  if (m_blockIndex > 0 && m_previousHash == "GENESIS") {
    return false;
  }

  return serialization::BlockSnapshotHeaderCodec::headerPayloadMatchesMetadata(
      m_headerPayload, m_blockIndex, m_previousHash, m_timestamp,
      m_recordCount);
}

std::string BlockSnapshotHeader::serialize() const {
  std::ostringstream oss;

  oss << "BlockSnapshotHeader{"
      << "blockIndex=" << m_blockIndex << ";previousHash=" << m_previousHash
      << ";blockHash=" << m_blockHash << ";timestamp=" << m_timestamp
      << ";recordCount=" << m_recordCount
      << ";calculatedHash=" << m_calculatedHash << "}";

  return oss.str();
}

std::string BlockSnapshotHeader::readFile(const std::string &filePath) {
  if (filePath.empty()) {
    throw std::invalid_argument("Block snapshot file path cannot be empty.");
  }

  std::ifstream input(filePath, std::ios::in | std::ios::binary);

  if (!input.is_open()) {
    throw std::runtime_error(
        "Failed to open block snapshot file for header parsing.");
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  if (!input.good() && !input.eof()) {
    throw std::runtime_error(
        "Failed while reading block snapshot for header parsing.");
  }

  return buffer.str();
}

bool BlockSnapshotHeader::isSafeHash(const std::string &hash) {
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

bool BlockSnapshotHeader::isSafePreviousHash(const std::string &previousHash) {
  if (previousHash == "GENESIS") {
    return true;
  }

  return isSafeHash(previousHash);
}

} // namespace nodo::storage