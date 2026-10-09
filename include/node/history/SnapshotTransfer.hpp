#ifndef NODO_NODE_HISTORY_SNAPSHOT_TRANSFER_HPP
#define NODO_NODE_HISTORY_SNAPSHOT_TRANSFER_HPP

#include "serialization/V1MerkleTree.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * Chunked snapshot transfer (ADR 0014, v1 SNAPSHOT_MANIFEST/SNAPSHOT_CHUNK).
 *
 * A checkpoint's snapshotDigest is the v1 Merkle root of the snapshot bytes
 * cut into fixed chunks, so every chunk is verified against the trusted
 * checkpoint as it arrives. The served manifest is advisory: a false chunk
 * count or size makes every chunk fail its proof. Memory grows only with
 * verified bytes, never with a peer's claimed size, and nothing is ever
 * decompressed.
 */
struct SnapshotChunk {
  std::uint64_t index = 0;
  std::vector<unsigned char> bytes;
  serialization::V1MerkleTree::InclusionProof proof;
};

class SnapshotChunkServer {
public:
  SnapshotChunkServer(std::vector<unsigned char> snapshotBytes,
                      std::uint32_t chunkBytes);

  std::uint64_t chunkCount() const;
  std::uint64_t totalBytes() const;
  std::string digest() const;
  SnapshotChunk chunk(std::uint64_t index) const;

private:
  std::vector<unsigned char> m_bytes;
  std::uint32_t m_chunkBytes;
  std::vector<serialization::V1MerkleTree::Digest> m_leaves;
};

enum class SnapshotChunkAcceptStatus {
  ACCEPTED,
  DUPLICATE,
  OUT_OF_RANGE,
  WRONG_LENGTH,
  INVALID_PROOF,
  OVER_BUDGET
};

std::string snapshotChunkAcceptStatusToString(SnapshotChunkAcceptStatus status);

class SnapshotAssembler {
public:
  // Rejects (throws) a transfer whose claimed geometry exceeds maxBytes or
  // the v1 leaf limit before any chunk is accepted.
  SnapshotAssembler(std::string expectedDigest, std::uint64_t claimedTotalBytes,
                    std::uint32_t chunkBytes, std::uint64_t maxBytes);

  SnapshotChunkAcceptStatus accept(const SnapshotChunk &chunk);
  bool complete() const;
  std::uint64_t receivedChunks() const;
  std::uint64_t chunkCount() const;
  // The assembled bytes; throws unless complete().
  std::vector<unsigned char> assemble() const;

private:
  serialization::V1MerkleTree::Digest m_expectedRoot;
  std::uint64_t m_totalBytes;
  std::uint32_t m_chunkBytes;
  std::uint64_t m_chunkCount;
  std::uint64_t m_receivedBytes = 0;
  std::vector<std::vector<unsigned char>> m_chunks;
  std::set<std::uint64_t> m_received;
};

} // namespace nodo::node

#endif
