#include "archive/ArchiveSegment.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "core/ProtocolLimits.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/SafeScalar.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nodo::archive {

namespace {

constexpr std::size_t kMaxCommitmentFieldBytes = 256;

bool isSafeChainId(const std::string &value) {
  return utils::isSafeIdentifier(value, 128, "_-.");
}

void appendBigEndian(std::vector<unsigned char> &out, std::uint64_t value,
                     int bytes) {
  for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
    out.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
  }
}

std::vector<unsigned char> framedBlock(std::uint64_t height,
                                       const std::string &blockBytes) {
  std::vector<unsigned char> framed;
  framed.reserve(config::HistoryParameters::kArchiveBlockFramingBytes +
                 blockBytes.size());
  appendBigEndian(framed, height, 8);
  appendBigEndian(framed, blockBytes.size(), 4);
  framed.insert(framed.end(), blockBytes.begin(), blockBytes.end());
  return framed;
}

class PieceAccumulator {
public:
  explicit PieceAccumulator(std::uint32_t pieceBytes)
      : m_pieceBytes(pieceBytes) {}

  void append(const std::vector<unsigned char> &bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
      const std::size_t take =
          std::min<std::size_t>(m_pieceBytes - m_piece.size(),
                                bytes.size() - offset);
      m_piece.insert(m_piece.end(), bytes.begin() + offset,
                     bytes.begin() + offset + take);
      offset += take;
      m_total += take;
      if (m_piece.size() == m_pieceBytes) {
        flush();
      }
    }
  }

  void finish() {
    if (!m_piece.empty()) {
      flush();
    }
  }

  std::uint64_t total() const { return m_total; }
  std::vector<serialization::V1MerkleTree::Digest> &leaves() {
    return m_leaves;
  }

private:
  void flush() {
    m_leaves.push_back(serialization::V1MerkleTree::leafHash(
        ArchiveSegmentBuilder::LEAF_KIND, m_leaves.size(), m_piece));
    m_piece.clear();
  }

  std::uint32_t m_pieceBytes;
  std::vector<unsigned char> m_piece;
  std::vector<serialization::V1MerkleTree::Digest> m_leaves;
  std::uint64_t m_total = 0;
};

} // namespace

ArchiveSegmentCommitment::ArchiveSegmentCommitment()
    : m_chainId(), m_segmentIndex(0), m_firstHeight(0), m_lastHeight(0),
      m_pieceBytes(0), m_totalBytes(0), m_pieceCount(0), m_pieceRoot(),
      m_lastBlockHash() {}

ArchiveSegmentCommitment::ArchiveSegmentCommitment(
    std::string chainId, std::uint64_t segmentIndex, std::uint64_t firstHeight,
    std::uint64_t lastHeight, std::uint32_t pieceBytes,
    std::uint64_t totalBytes, std::uint64_t pieceCount, std::string pieceRoot,
    std::string lastBlockHash)
    : m_chainId(std::move(chainId)), m_segmentIndex(segmentIndex),
      m_firstHeight(firstHeight), m_lastHeight(lastHeight),
      m_pieceBytes(pieceBytes), m_totalBytes(totalBytes),
      m_pieceCount(pieceCount), m_pieceRoot(std::move(pieceRoot)),
      m_lastBlockHash(std::move(lastBlockHash)) {}

const std::string &ArchiveSegmentCommitment::chainId() const {
  return m_chainId;
}
std::uint64_t ArchiveSegmentCommitment::segmentIndex() const {
  return m_segmentIndex;
}
std::uint64_t ArchiveSegmentCommitment::firstHeight() const {
  return m_firstHeight;
}
std::uint64_t ArchiveSegmentCommitment::lastHeight() const {
  return m_lastHeight;
}
std::uint32_t ArchiveSegmentCommitment::pieceBytes() const {
  return m_pieceBytes;
}
std::uint64_t ArchiveSegmentCommitment::totalBytes() const {
  return m_totalBytes;
}
std::uint64_t ArchiveSegmentCommitment::pieceCount() const {
  return m_pieceCount;
}
const std::string &ArchiveSegmentCommitment::pieceRoot() const {
  return m_pieceRoot;
}
const std::string &ArchiveSegmentCommitment::lastBlockHash() const {
  return m_lastBlockHash;
}

bool ArchiveSegmentCommitment::isValid() const {
  if (!isSafeChainId(m_chainId) || m_firstHeight == 0 ||
      m_lastHeight < m_firstHeight || m_pieceBytes == 0 || m_totalBytes == 0 ||
      m_pieceCount == 0 ||
      m_pieceCount > serialization::V1MerkleTree::kMaxLeaves ||
      !isDigestHex(m_pieceRoot) || !isDigestHex(m_lastBlockHash)) {
    return false;
  }
  const std::uint64_t blocks = m_lastHeight - m_firstHeight + 1;
  if (blocks > m_totalBytes /
                   config::HistoryParameters::kArchiveBlockFramingBytes) {
    return false;
  }
  return m_pieceCount == (m_totalBytes - 1) / m_pieceBytes + 1;
}

bool ArchiveSegmentCommitment::matchesParameters(
    const config::HistoryParameters &parameters) const {
  try {
    return isValid() && parameters.isValid() &&
           m_pieceBytes == parameters.archivePieceBytes() &&
           m_firstHeight == parameters.segmentFirstHeight(m_segmentIndex) &&
           m_lastHeight == parameters.segmentLastHeight(m_segmentIndex);
  } catch (const std::exception &) {
    return false;
  }
}

std::uint64_t
ArchiveSegmentCommitment::pieceLength(std::uint64_t pieceIndex) const {
  if (!isValid() || pieceIndex >= m_pieceCount) {
    throw std::out_of_range("Archive piece index is outside the segment.");
  }
  if (pieceIndex + 1 < m_pieceCount) {
    return m_pieceBytes;
  }
  return m_totalBytes - static_cast<std::uint64_t>(m_pieceBytes) * pieceIndex;
}

std::string ArchiveSegmentCommitment::segmentId() const {
  return hashHex("ARCHIVE/SEGMENT-ID", encode());
}

std::vector<unsigned char> ArchiveSegmentCommitment::encode() const {
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeString(m_chainId);
  writer.writeUInt64(m_segmentIndex);
  writer.writeUInt64(m_firstHeight);
  writer.writeUInt64(m_lastHeight);
  writer.writeUInt32(m_pieceBytes);
  writer.writeUInt64(m_totalBytes);
  writer.writeUInt64(m_pieceCount);
  writer.writeString(m_pieceRoot);
  writer.writeString(m_lastBlockHash);
  return writer.bytes();
}

ArchiveSegmentCommitment
ArchiveSegmentCommitment::decode(const std::vector<unsigned char> &bytes) {
  serialization::CanonicalReader reader(bytes, kMaxCommitmentFieldBytes);
  if (reader.readString() != SCHEMA) {
    throw std::invalid_argument("Unknown archive segment commitment schema.");
  }
  std::string chainId = reader.readString();
  const std::uint64_t segmentIndex = reader.readUInt64();
  const std::uint64_t firstHeight = reader.readUInt64();
  const std::uint64_t lastHeight = reader.readUInt64();
  const std::uint32_t pieceBytes = reader.readUInt32();
  const std::uint64_t totalBytes = reader.readUInt64();
  const std::uint64_t pieceCount = reader.readUInt64();
  std::string pieceRoot = reader.readString();
  std::string lastBlockHash = reader.readString();
  reader.requireFullyConsumed();
  ArchiveSegmentCommitment commitment(
      std::move(chainId), segmentIndex, firstHeight, lastHeight, pieceBytes,
      totalBytes, pieceCount, std::move(pieceRoot), std::move(lastBlockHash));
  if (!commitment.isValid()) {
    throw std::invalid_argument("Archive segment commitment is invalid.");
  }
  return commitment;
}

ArchiveSegmentIndex::ArchiveSegmentIndex()
    : m_commitment(), m_leaves(), m_blockOffsets() {}

ArchiveSegmentIndex::ArchiveSegmentIndex(
    ArchiveSegmentCommitment commitment,
    std::vector<serialization::V1MerkleTree::Digest> leaves,
    std::vector<std::uint64_t> blockOffsets)
    : m_commitment(std::move(commitment)), m_leaves(std::move(leaves)),
      m_blockOffsets(std::move(blockOffsets)) {}

const ArchiveSegmentCommitment &ArchiveSegmentIndex::commitment() const {
  return m_commitment;
}
const std::vector<serialization::V1MerkleTree::Digest> &
ArchiveSegmentIndex::leaves() const {
  return m_leaves;
}
const std::vector<std::uint64_t> &ArchiveSegmentIndex::blockOffsets() const {
  return m_blockOffsets;
}

ArchiveSegmentIndex
ArchiveSegmentBuilder::build(const config::HistoryParameters &parameters,
                             const std::string &chainId,
                             std::uint64_t segmentIndex,
                             const ArchivedBlockSource &source) {
  if (!parameters.isValid() || !isSafeChainId(chainId) || !source) {
    throw std::invalid_argument("Invalid archive segment build input.");
  }
  const std::uint64_t first = parameters.segmentFirstHeight(segmentIndex);
  const std::uint64_t last = parameters.segmentLastHeight(segmentIndex);

  PieceAccumulator pieces(parameters.archivePieceBytes());
  std::vector<std::uint64_t> offsets;
  offsets.reserve(static_cast<std::size_t>(last - first + 1));
  std::string lastBlockHash;
  for (std::uint64_t height = first; height <= last; ++height) {
    const std::optional<ArchivedBlock> block = source(height);
    if (!block) {
      throw std::runtime_error("Archive segment is missing block " +
                               std::to_string(height) + ".");
    }
    if (block->blockBytes.empty() ||
        block->blockBytes.size() >
            core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES ||
        !isDigestHex(block->blockHash)) {
      throw std::runtime_error("Archive segment block " +
                               std::to_string(height) + " is malformed.");
    }
    offsets.push_back(pieces.total());
    pieces.append(framedBlock(height, block->blockBytes));
    lastBlockHash = block->blockHash;
  }
  pieces.finish();

  const std::string root = serialization::V1EncodingPrimitives::hex(
      serialization::V1MerkleTree::rootFromLeafHashes(LEAF_KIND,
                                                      pieces.leaves()));
  ArchiveSegmentCommitment commitment(
      chainId, segmentIndex, first, last, parameters.archivePieceBytes(),
      pieces.total(), pieces.leaves().size(), root, lastBlockHash);
  if (!commitment.matchesParameters(parameters)) {
    throw std::logic_error("Archive segment builder produced an invalid "
                           "commitment.");
  }
  return ArchiveSegmentIndex(std::move(commitment), std::move(pieces.leaves()),
                             std::move(offsets));
}

std::vector<unsigned char>
ArchiveSegmentBuilder::readPiece(const ArchiveSegmentIndex &index,
                                 std::uint64_t pieceIndex,
                                 const ArchivedBlockSource &source) {
  const ArchiveSegmentCommitment &commitment = index.commitment();
  const std::uint64_t length = commitment.pieceLength(pieceIndex);
  const std::uint64_t start =
      static_cast<std::uint64_t>(commitment.pieceBytes()) * pieceIndex;
  const std::uint64_t end = start + length;
  const std::vector<std::uint64_t> &offsets = index.blockOffsets();
  if (!source || offsets.empty() ||
      offsets.size() !=
          commitment.lastHeight() - commitment.firstHeight() + 1) {
    throw std::invalid_argument("Archive segment index has no block offsets.");
  }

  std::size_t block = static_cast<std::size_t>(
      std::upper_bound(offsets.begin(), offsets.end(), start) -
      offsets.begin() - 1);
  std::vector<unsigned char> piece;
  piece.reserve(static_cast<std::size_t>(length));
  while (piece.size() < length) {
    if (block >= offsets.size()) {
      throw std::runtime_error("Archive piece extends past the segment.");
    }
    const std::uint64_t height = commitment.firstHeight() + block;
    const std::optional<ArchivedBlock> archived = source(height);
    if (!archived) {
      throw std::runtime_error("Archived block " + std::to_string(height) +
                               " is missing.");
    }
    const std::vector<unsigned char> framed =
        framedBlock(height, archived->blockBytes);
    const std::uint64_t blockStart = offsets[block];
    const std::uint64_t blockEnd = block + 1 < offsets.size()
                                       ? offsets[block + 1]
                                       : commitment.totalBytes();
    if (blockEnd - blockStart != framed.size()) {
      throw std::runtime_error("Archived block " + std::to_string(height) +
                               " no longer matches the segment index.");
    }
    const std::uint64_t from = std::max(start, blockStart) - blockStart;
    const std::uint64_t to = std::min(end, blockEnd) - blockStart;
    piece.insert(piece.end(), framed.begin() + static_cast<std::ptrdiff_t>(from),
                 framed.begin() + static_cast<std::ptrdiff_t>(to));
    ++block;
  }
  return piece;
}

bool ArchiveIndex::isContiguous(
    const std::vector<ArchiveSegmentCommitment> &ordered) {
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    if (!ordered[index].isValid() || ordered[index].segmentIndex() != index ||
        ordered[index].chainId() != ordered.front().chainId()) {
      return false;
    }
  }
  return true;
}

std::string
ArchiveIndex::root(const std::vector<ArchiveSegmentCommitment> &ordered) {
  if (!isContiguous(ordered)) {
    throw std::invalid_argument("Archive index must be contiguous from zero.");
  }
  std::vector<std::vector<unsigned char>> elements;
  elements.reserve(ordered.size());
  for (const ArchiveSegmentCommitment &commitment : ordered) {
    elements.push_back(commitment.encode());
  }
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1MerkleTree::root(LEAF_KIND, elements));
}

serialization::V1MerkleTree::InclusionProof
ArchiveIndex::prove(const std::vector<ArchiveSegmentCommitment> &ordered,
                    std::uint64_t segmentIndex) {
  if (!isContiguous(ordered) || segmentIndex >= ordered.size()) {
    throw std::invalid_argument("Archive index proof input is invalid.");
  }
  std::vector<serialization::V1MerkleTree::Digest> leaves;
  leaves.reserve(ordered.size());
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    leaves.push_back(serialization::V1MerkleTree::leafHash(
        LEAF_KIND, index, ordered[index].encode()));
  }
  return serialization::V1MerkleTree::prove(LEAF_KIND, leaves,
                                            {segmentIndex})
      .front();
}

bool ArchiveIndex::verifyMembership(
    const std::string &indexRoot, const ArchiveSegmentCommitment &commitment,
    const serialization::V1MerkleTree::InclusionProof &proof) {
  const auto root = serialization::V1MerkleTree::digestFromHex(indexRoot);
  if (!root || !commitment.isValid() ||
      proof.leafIndex != commitment.segmentIndex()) {
    return false;
  }
  return serialization::V1MerkleTree::verify(LEAF_KIND, *root, proof,
                                             commitment.encode());
}

} // namespace nodo::archive
