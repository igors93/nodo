#ifndef NODO_ARCHIVE_ARCHIVE_SEGMENT_HPP
#define NODO_ARCHIVE_ARCHIVE_SEGMENT_HPP

#include "config/HistoryParameters.hpp"
#include "serialization/V1MerkleTree.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nodo::archive {

/*
 * ArchiveSegmentCommitment commits to one sealed range of finalized blocks
 * (ADR 0014). The archived byte stream is, for every height in order,
 * u64(height) || u32(length) || canonical block bytes. The stream is cut into
 * fixed-size pieces (the last piece may be shorter) and the pieces form the
 * leaves of an ordered v1 Merkle tree of kind "archive". Challenges sample
 * pieces, so an archival proof never needs to ship a whole block.
 *
 * Every node that still holds the blocks computes the same commitment, and
 * finalized state checkpoints commit the ordered list of sealed segment
 * commitments. lastBlockHash ties the segment to the finalized header chain.
 */
class ArchiveSegmentCommitment {
public:
  static constexpr const char *SCHEMA = "NODO_ARCHIVE_SEGMENT_COMMITMENT_V1";

  ArchiveSegmentCommitment();
  ArchiveSegmentCommitment(std::string chainId, std::uint64_t segmentIndex,
                           std::uint64_t firstHeight, std::uint64_t lastHeight,
                           std::uint32_t pieceBytes, std::uint64_t totalBytes,
                           std::uint64_t pieceCount, std::string pieceRoot,
                           std::string lastBlockHash);

  const std::string &chainId() const;
  std::uint64_t segmentIndex() const;
  std::uint64_t firstHeight() const;
  std::uint64_t lastHeight() const;
  std::uint32_t pieceBytes() const;
  std::uint64_t totalBytes() const;
  std::uint64_t pieceCount() const;
  const std::string &pieceRoot() const;
  const std::string &lastBlockHash() const;

  bool isValid() const;
  // Heights and piece geometry agree with the network parameters.
  bool matchesParameters(const config::HistoryParameters &parameters) const;
  // Exact byte length piece pieceIndex must have.
  std::uint64_t pieceLength(std::uint64_t pieceIndex) const;

  std::string segmentId() const;
  std::vector<unsigned char> encode() const;
  static ArchiveSegmentCommitment decode(const std::vector<unsigned char> &bytes);

private:
  std::string m_chainId;
  std::uint64_t m_segmentIndex;
  std::uint64_t m_firstHeight;
  std::uint64_t m_lastHeight;
  std::uint32_t m_pieceBytes;
  std::uint64_t m_totalBytes;
  std::uint64_t m_pieceCount;
  std::string m_pieceRoot;
  std::string m_lastBlockHash;
};

// Everything an archive provider needs to answer challenges for a segment
// besides the block bytes themselves: piece leaf hashes (to build proofs) and
// the stream offset at which each block's framing starts.
class ArchiveSegmentIndex {
public:
  ArchiveSegmentIndex();
  ArchiveSegmentIndex(ArchiveSegmentCommitment commitment,
                      std::vector<serialization::V1MerkleTree::Digest> leaves,
                      std::vector<std::uint64_t> blockOffsets);

  const ArchiveSegmentCommitment &commitment() const;
  const std::vector<serialization::V1MerkleTree::Digest> &leaves() const;
  const std::vector<std::uint64_t> &blockOffsets() const;

private:
  ArchiveSegmentCommitment m_commitment;
  std::vector<serialization::V1MerkleTree::Digest> m_leaves;
  std::vector<std::uint64_t> m_blockOffsets;
};

struct ArchivedBlock {
  std::string blockHash;
  std::string blockBytes;
};

// Returns the canonical block at a height, or nullopt when it is missing.
using ArchivedBlockSource =
    std::function<std::optional<ArchivedBlock>(std::uint64_t height)>;

class ArchiveSegmentBuilder {
public:
  static constexpr const char *LEAF_KIND = "archive";

  // Streams the whole segment. Throws std::runtime_error when any block is
  // missing, out of order or larger than the protocol block limit.
  static ArchiveSegmentIndex build(const config::HistoryParameters &parameters,
                                   const std::string &chainId,
                                   std::uint64_t segmentIndex,
                                   const ArchivedBlockSource &source);

  // Rebuilds the bytes of one piece from the blocks it overlaps.
  static std::vector<unsigned char>
  readPiece(const ArchiveSegmentIndex &index, std::uint64_t pieceIndex,
            const ArchivedBlockSource &source);
};

// The ordered list of sealed segment commitments, committed by checkpoints
// as a v1 Merkle root of kind "archive-index".
class ArchiveIndex {
public:
  static constexpr const char *LEAF_KIND = "archive-index";

  // Commitments must be contiguous from segment 0 and share one chain id.
  static bool isContiguous(const std::vector<ArchiveSegmentCommitment> &ordered);
  static std::string root(const std::vector<ArchiveSegmentCommitment> &ordered);
  static serialization::V1MerkleTree::InclusionProof
  prove(const std::vector<ArchiveSegmentCommitment> &ordered,
        std::uint64_t segmentIndex);
  static bool verifyMembership(const std::string &indexRoot,
                               const ArchiveSegmentCommitment &commitment,
                               const serialization::V1MerkleTree::InclusionProof
                                   &proof);
};

} // namespace nodo::archive

#endif
