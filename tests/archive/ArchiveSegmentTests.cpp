#include "ArchiveTestSupport.hpp"

#include <iostream>

using nodo::test::require;
using nodo::test::SyntheticHistory;
using namespace nodo;
using namespace nodo::archive;

namespace {

void testCommitmentIsDeterministic() {
  const SyntheticHistory history(16);
  const ArchiveSegmentIndex first = history.segment(1);
  const ArchiveSegmentIndex again = history.segment(1);
  const ArchiveSegmentCommitment &commitment = first.commitment();
  require(commitment.encode() == again.commitment().encode() &&
              commitment.segmentId() == again.commitment().segmentId(),
          "segment commitments are deterministic");
  require(commitment.firstHeight() == 5 && commitment.lastHeight() == 8 &&
              commitment.matchesParameters(history.parameters()),
          "segment 1 holds heights 5..8");
  require(commitment.lastBlockHash() == history.blockHash(8),
          "the commitment binds the last finalized block");
  require(commitment.pieceCount() == first.leaves().size() &&
              commitment.pieceCount() > 1,
          "the segment spans several pieces");

  const ArchiveSegmentCommitment decoded =
      ArchiveSegmentCommitment::decode(commitment.encode());
  require(decoded.encode() == commitment.encode(), "commitment round-trips");
  std::vector<unsigned char> trailing = commitment.encode();
  trailing.push_back(0);
  bool rejected = false;
  try {
    (void)ArchiveSegmentCommitment::decode(trailing);
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, "trailing bytes are rejected");
}

void testPiecesReconstructTheCommitment() {
  const SyntheticHistory history(8);
  const ArchiveSegmentIndex segment = history.segment(1);
  std::uint64_t total = 0;
  for (std::uint64_t piece = 0; piece < segment.commitment().pieceCount(); ++piece) {
    const auto bytes =
        ArchiveSegmentBuilder::readPiece(segment, piece, history.source());
    require(bytes.size() == segment.commitment().pieceLength(piece),
            "piece length matches the commitment");
    require(serialization::V1MerkleTree::leafHash("archive", piece, bytes) ==
                segment.leaves()[piece],
            "a piece read back from blocks hashes to its leaf");
    total += bytes.size();
  }
  require(total == segment.commitment().totalBytes(),
          "pieces cover exactly the segment stream");
}

void testAnyChangeChangesTheCommitment() {
  const SyntheticHistory history(8);
  const std::string root = history.segment(1).commitment().pieceRoot();
  // Same blocks with one byte flipped in one block.
  const auto tampered = [&history](std::uint64_t height)
      -> std::optional<ArchivedBlock> {
    auto block = history.source()(height);
    if (block && height == 6) {
      block->blockBytes[block->blockBytes.size() / 2] ^= 0x01;
    }
    return block;
  };
  require(ArchiveSegmentBuilder::build(history.parameters(),
                                       SyntheticHistory::kChainId, 1, tampered)
                  .commitment()
                  .pieceRoot() != root,
          "one changed byte changes the segment root");

  bool missing = false;
  try {
    (void)ArchiveSegmentBuilder::build(history.parameters(),
                                       SyntheticHistory::kChainId, 1,
                                       history.source({7}));
  } catch (const std::runtime_error &) {
    missing = true;
  }
  require(missing, "a segment cannot be committed with a missing block");
}

void testArchiveIndexMembership() {
  const SyntheticHistory history(16);
  const std::vector<ArchiveSegmentCommitment> commitments = history.commitments(4);
  require(ArchiveIndex::isContiguous(commitments), "index is contiguous");
  const std::string root = ArchiveIndex::root(commitments);
  for (std::uint64_t index = 0; index < commitments.size(); ++index) {
    const auto proof = ArchiveIndex::prove(commitments, index);
    require(ArchiveIndex::verifyMembership(root, commitments[index], proof),
            "every sealed segment is provably in the index");
    require(!ArchiveIndex::verifyMembership(root,
                                            commitments[(index + 1) % 4], proof),
            "a proof binds its own segment");
  }
  std::vector<ArchiveSegmentCommitment> gap = commitments;
  gap.erase(gap.begin() + 1);
  require(!ArchiveIndex::isContiguous(gap), "an index with a gap is rejected");
}

void testParameterGeometryIsBounded() {
  const SyntheticHistory history(4);
  const ArchiveSegmentCommitment commitment = history.segment(0).commitment();
  config::HistoryParameterValues values = history.parameters().values();
  values.archivePieceBytes = 1024;
  require(!commitment.matchesParameters(config::HistoryParameters(values)),
          "a commitment cut with another piece size is rejected");
}

} // namespace

int main() {
  try {
    testCommitmentIsDeterministic();
    testPiecesReconstructTheCommitment();
    testAnyChangeChangesTheCommitment();
    testArchiveIndexMembership();
    testParameterGeometryIsBounded();
    std::cout << "Archive segment tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Archive segment tests failed: " << error.what() << "\n";
    return 1;
  }
}
