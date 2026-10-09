#include "node/history/SnapshotTransfer.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr const char *kChunkKind = "snapshot-chunk";

std::uint64_t chunksFor(std::uint64_t totalBytes, std::uint32_t chunkBytes) {
  return totalBytes == 0 ? 0 : (totalBytes - 1) / chunkBytes + 1;
}

std::uint64_t expectedLength(std::uint64_t index, std::uint64_t count,
                             std::uint64_t totalBytes,
                             std::uint32_t chunkBytes) {
  return index + 1 < count ? chunkBytes : totalBytes - index * chunkBytes;
}

} // namespace

SnapshotChunkServer::SnapshotChunkServer(std::vector<unsigned char> snapshotBytes,
                                         std::uint32_t chunkBytes)
    : m_bytes(std::move(snapshotBytes)), m_chunkBytes(chunkBytes), m_leaves() {
  if (m_bytes.empty() || m_chunkBytes == 0 ||
      chunksFor(m_bytes.size(), m_chunkBytes) >
          serialization::V1MerkleTree::kMaxLeaves) {
    throw std::invalid_argument("Snapshot cannot be served in bounded chunks.");
  }
  const std::span<const unsigned char> all(m_bytes);
  const std::uint64_t count = chunkCount();
  m_leaves.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    const std::size_t offset = static_cast<std::size_t>(index * m_chunkBytes);
    const std::size_t length = static_cast<std::size_t>(
        expectedLength(index, count, m_bytes.size(), m_chunkBytes));
    m_leaves.push_back(serialization::V1MerkleTree::leafHash(
        kChunkKind, index, all.subspan(offset, length)));
  }
}

std::uint64_t SnapshotChunkServer::chunkCount() const {
  return chunksFor(m_bytes.size(), m_chunkBytes);
}

std::uint64_t SnapshotChunkServer::totalBytes() const { return m_bytes.size(); }

std::string SnapshotChunkServer::digest() const {
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1MerkleTree::rootFromLeafHashes(kChunkKind, m_leaves));
}

SnapshotChunk SnapshotChunkServer::chunk(std::uint64_t index) const {
  const std::uint64_t count = chunkCount();
  if (index >= count) {
    throw std::out_of_range("Snapshot chunk index is out of range.");
  }
  SnapshotChunk chunk;
  chunk.index = index;
  const std::size_t offset = static_cast<std::size_t>(index * m_chunkBytes);
  const std::size_t length = static_cast<std::size_t>(
      expectedLength(index, count, m_bytes.size(), m_chunkBytes));
  chunk.bytes.assign(m_bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                     m_bytes.begin() +
                         static_cast<std::ptrdiff_t>(offset + length));
  chunk.proof =
      serialization::V1MerkleTree::prove(kChunkKind, m_leaves, {index}).front();
  return chunk;
}

std::string
snapshotChunkAcceptStatusToString(SnapshotChunkAcceptStatus status) {
  switch (status) {
  case SnapshotChunkAcceptStatus::ACCEPTED:
    return "ACCEPTED";
  case SnapshotChunkAcceptStatus::DUPLICATE:
    return "DUPLICATE";
  case SnapshotChunkAcceptStatus::OUT_OF_RANGE:
    return "OUT_OF_RANGE";
  case SnapshotChunkAcceptStatus::WRONG_LENGTH:
    return "WRONG_LENGTH";
  case SnapshotChunkAcceptStatus::INVALID_PROOF:
    return "INVALID_PROOF";
  case SnapshotChunkAcceptStatus::OVER_BUDGET:
    return "OVER_BUDGET";
  }
  return "INVALID_PROOF";
}

SnapshotAssembler::SnapshotAssembler(std::string expectedDigest,
                                     std::uint64_t claimedTotalBytes,
                                     std::uint32_t chunkBytes,
                                     std::uint64_t maxBytes)
    : m_expectedRoot(), m_totalBytes(claimedTotalBytes),
      m_chunkBytes(chunkBytes), m_chunkCount(0), m_chunks(), m_received() {
  const auto root = serialization::V1MerkleTree::digestFromHex(expectedDigest);
  if (!root || chunkBytes == 0 || claimedTotalBytes == 0 ||
      claimedTotalBytes > maxBytes ||
      chunksFor(claimedTotalBytes, chunkBytes) >
          serialization::V1MerkleTree::kMaxLeaves) {
    throw std::invalid_argument("Snapshot transfer geometry is not acceptable.");
  }
  m_expectedRoot = *root;
  m_chunkCount = chunksFor(claimedTotalBytes, chunkBytes);
  // Slots only; chunk bytes are stored once verified.
  m_chunks.resize(static_cast<std::size_t>(m_chunkCount));
}

SnapshotChunkAcceptStatus SnapshotAssembler::accept(const SnapshotChunk &chunk) {
  if (chunk.index >= m_chunkCount || chunk.proof.leafIndex != chunk.index ||
      chunk.proof.treeSize != m_chunkCount) {
    return SnapshotChunkAcceptStatus::OUT_OF_RANGE;
  }
  if (m_received.count(chunk.index) != 0) {
    return SnapshotChunkAcceptStatus::DUPLICATE;
  }
  if (chunk.bytes.size() != expectedLength(chunk.index, m_chunkCount,
                                           m_totalBytes, m_chunkBytes)) {
    return SnapshotChunkAcceptStatus::WRONG_LENGTH;
  }
  if (m_receivedBytes + chunk.bytes.size() > m_totalBytes) {
    return SnapshotChunkAcceptStatus::OVER_BUDGET;
  }
  if (!serialization::V1MerkleTree::verify(kChunkKind, m_expectedRoot,
                                           chunk.proof, chunk.bytes)) {
    return SnapshotChunkAcceptStatus::INVALID_PROOF;
  }
  m_chunks[static_cast<std::size_t>(chunk.index)] = chunk.bytes;
  m_received.insert(chunk.index);
  m_receivedBytes += chunk.bytes.size();
  return SnapshotChunkAcceptStatus::ACCEPTED;
}

bool SnapshotAssembler::complete() const {
  return m_received.size() == m_chunkCount && m_receivedBytes == m_totalBytes;
}

std::uint64_t SnapshotAssembler::receivedChunks() const {
  return m_received.size();
}

std::uint64_t SnapshotAssembler::chunkCount() const { return m_chunkCount; }

std::vector<unsigned char> SnapshotAssembler::assemble() const {
  if (!complete()) {
    throw std::logic_error("Snapshot transfer is not complete.");
  }
  std::vector<unsigned char> bytes;
  bytes.reserve(static_cast<std::size_t>(m_totalBytes));
  for (const auto &chunk : m_chunks) {
    bytes.insert(bytes.end(), chunk.begin(), chunk.end());
  }
  return bytes;
}

} // namespace nodo::node
