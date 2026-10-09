#include "../../common/HistoryChainFixture.hpp"
#include "../../common/TestFramework.hpp"

#include "config/HistoryParameters.hpp"
#include "node/history/CheckpointBootstrap.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"
#include "node/history/HistoryStore.hpp"
#include "node/history/SnapshotTransfer.hpp"

#include <algorithm>
#include <iostream>

using nodo::test::HistoryChainFixture;
using nodo::test::require;
using namespace nodo;
using namespace nodo::node;

namespace {

const config::HistoryParameters &parameters() {
  static const config::HistoryParameters params =
      config::HistoryParameters::developmentLocal();
  return params;
}

struct PeerData {
  FinalizedStateCheckpoint checkpoint;
  std::vector<unsigned char> snapshot;
  std::vector<BootstrapFinalizedBlock> laterBlocks;
};

PeerData servedBy(const HistoryChainFixture &chain, std::uint64_t height) {
  const HistoryStore store(chain.directory());
  PeerData data;
  data.checkpoint = *store.loadCheckpoint(height);
  data.snapshot = *store.loadSnapshotBytes(height, parameters().maxSnapshotBytes());
  for (std::uint64_t next = height + 1; next <= chain.height(); ++next) {
    const FinalizedBlockArtifact artifact = chain.artifact(next);
    data.laterBlocks.push_back({artifact.block(), artifact.finalizedRecord()});
  }
  return data;
}

std::int64_t nowFor(const PeerData &data) {
  return data.checkpoint.fields().blockTimestamp + 60;
}

TrustedCheckpointAnchor anchorFor(const FinalizedStateCheckpoint &checkpoint) {
  return TrustedCheckpointAnchor{checkpoint.height(), checkpoint.checkpointId()};
}

CheckpointBootstrapResult bootstrap(const TrustedCheckpointAnchor &anchor,
                                    const PeerData &data, std::int64_t now) {
  return CheckpointBootstrapVerifier::verify(
      HistoryChainFixture::genesis(), parameters(), anchor, data.checkpoint,
      data.snapshot, data.laterBlocks, now);
}

void requireRejected(const CheckpointBootstrapResult &result,
                     CheckpointVerificationStatus expected,
                     const std::string &what) {
  require(!result.verified && result.status == expected && !result.state,
          what + ": got " + checkpointVerificationStatusToString(result.status) +
              " (" + result.reason + ")");
}

void testAnchorParsing() {
  const std::string id(64, 'a');
  const auto anchor = TrustedCheckpointAnchor::parse("16:" + id);
  require(anchor && anchor->height == 16 && anchor->checkpointId == id,
          "anchor parses");
  for (const std::string bad :
       {std::string("16"), ":" + id, "016:" + id, "16:" + id + ":x",
        "16:" + std::string(63, 'a'), "16:" + std::string(64, 'G'),
        "x:" + id, "16:" + std::string(64, '0')}) {
    require(!TrustedCheckpointAnchor::parse(bad).has_value(),
            "anchor must reject '" + bad + "'");
  }
}

void testHonestBootstrapReachesTip(const HistoryChainFixture &chain) {
  const PeerData data = servedBy(chain, 8);
  const CheckpointBootstrapResult result =
      bootstrap(anchorFor(data.checkpoint), data, nowFor(data));
  require(result.verified, "honest bootstrap must verify: " + result.reason);
  const core::Block tip = chain.artifact(chain.height()).block();
  require(result.verifiedHeight == chain.height() &&
              result.verifiedBlockHash == tip.hash() &&
              result.verifiedStateRoot == tip.stateRoot() &&
              result.replayedBlocks == chain.height() - 8,
          "bootstrap replays every later block up to the tip");
  require(result.state && result.state->stateRoot == tip.stateRoot(),
          "the verified state is the tip state");

  // A newer checkpoint needs fewer blocks.
  const PeerData recent = servedBy(chain, 16);
  const CheckpointBootstrapResult fromRecent =
      bootstrap(anchorFor(recent.checkpoint), recent, nowFor(recent));
  require(fromRecent.verified && fromRecent.verifiedStateRoot == tip.stateRoot(),
          "bootstrap from the latest checkpoint reaches the same tip");
}

void testMaliciousPeerIsRejected(const HistoryChainFixture &chain) {
  const PeerData honest = servedBy(chain, 8);
  const TrustedCheckpointAnchor anchor = anchorFor(honest.checkpoint);
  const std::int64_t now = nowFor(honest);

  requireRejected(bootstrap(TrustedCheckpointAnchor{}, honest, now),
                  CheckpointVerificationStatus::ANCHOR_MISMATCH,
                  "no anchor at all");
  requireRejected(
      bootstrap(TrustedCheckpointAnchor{8, std::string(64, 'e')}, honest, now),
      CheckpointVerificationStatus::ANCHOR_MISMATCH, "checkpoint not anchored");

  PeerData otherCheckpoint = honest;
  otherCheckpoint.checkpoint = servedBy(chain, 16).checkpoint;
  requireRejected(bootstrap(anchor, otherCheckpoint, now),
                  CheckpointVerificationStatus::ANCHOR_MISMATCH,
                  "peer serves a different checkpoint");

  requireRejected(
      bootstrap(anchor, honest,
                honest.checkpoint.fields().blockTimestamp +
                    parameters().weakSubjectivitySeconds() + 1),
      CheckpointVerificationStatus::STALE_ANCHOR,
      "anchor older than the weak-subjectivity window");

  PeerData wrongSnapshot = honest;
  wrongSnapshot.snapshot = servedBy(chain, 16).snapshot;
  requireRejected(bootstrap(anchor, wrongSnapshot, now),
                  CheckpointVerificationStatus::SNAPSHOT_DIGEST_MISMATCH,
                  "snapshot of another height");

  PeerData truncated = honest;
  truncated.snapshot.pop_back();
  requireRejected(bootstrap(anchor, truncated, now),
                  CheckpointVerificationStatus::SNAPSHOT_DIGEST_MISMATCH,
                  "truncated snapshot");

  PeerData gigantic = honest;
  gigantic.snapshot.assign(parameters().maxSnapshotBytes() + 1, 0x42);
  requireRejected(bootstrap(anchor, gigantic, now),
                  CheckpointVerificationStatus::SNAPSHOT_DIGEST_MISMATCH,
                  "oversized snapshot");

  PeerData gap = honest;
  gap.laterBlocks.erase(gap.laterBlocks.begin());
  requireRejected(bootstrap(anchor, gap, now),
                  CheckpointVerificationStatus::BLOCK_MISMATCH,
                  "missing block after the checkpoint");

  PeerData reordered = honest;
  std::swap(reordered.laterBlocks[0], reordered.laterBlocks[1]);
  requireRejected(bootstrap(anchor, reordered, now),
                  CheckpointVerificationStatus::BLOCK_MISMATCH,
                  "reordered blocks");

  PeerData forgedQc = honest;
  forgedQc.laterBlocks[2].finalizedRecord =
      forgedQc.laterBlocks[3].finalizedRecord;
  requireRejected(bootstrap(anchor, forgedQc, now),
                  CheckpointVerificationStatus::CERTIFICATE_INVALID,
                  "QC of another block");
}

void testChunkedSnapshotTransfer(const HistoryChainFixture &chain) {
  const PeerData honest = servedBy(chain, 16);
  const SnapshotChunkServer server(honest.snapshot,
                                   parameters().snapshotChunkBytes());
  require(server.digest() == honest.checkpoint.fields().snapshotDigest,
          "chunk Merkle root is the checkpoint's snapshot digest");

  SnapshotAssembler assembler(honest.checkpoint.fields().snapshotDigest,
                              server.totalBytes(),
                              parameters().snapshotChunkBytes(),
                              parameters().maxSnapshotBytes());
  std::vector<std::uint64_t> order;
  for (std::uint64_t index = 0; index < server.chunkCount(); ++index) {
    order.push_back(index);
  }
  std::reverse(order.begin(), order.end());
  for (const std::uint64_t index : order) {
    SnapshotChunk chunk = server.chunk(index);
    if (index == order.front() && !chunk.bytes.empty()) {
      SnapshotChunk tampered = chunk;
      tampered.bytes[0] ^= 0x01;
      require(assembler.accept(tampered) ==
                  SnapshotChunkAcceptStatus::INVALID_PROOF,
              "a tampered chunk fails its proof");
      SnapshotChunk shortened = chunk;
      shortened.bytes.pop_back();
      require(assembler.accept(shortened) !=
                  SnapshotChunkAcceptStatus::ACCEPTED,
              "a resized chunk is rejected");
    }
    require(assembler.accept(chunk) == SnapshotChunkAcceptStatus::ACCEPTED,
            "an honest chunk is accepted");
    require(assembler.accept(chunk) == SnapshotChunkAcceptStatus::DUPLICATE,
            "a repeated chunk is a duplicate");
  }
  require(assembler.complete() && assembler.assemble() == honest.snapshot,
          "assembled snapshot is byte-identical");

  // Multi-chunk transfer with the smallest chunk size.
  const SnapshotChunkServer small(honest.snapshot, 4096);
  require(small.chunkCount() > 1, "fixture snapshot spans several chunks");
  SnapshotAssembler smallAssembler(small.digest(), small.totalBytes(), 4096,
                                   parameters().maxSnapshotBytes());
  for (std::uint64_t index = small.chunkCount(); index-- > 0;) {
    require(smallAssembler.accept(small.chunk(index)) ==
                SnapshotChunkAcceptStatus::ACCEPTED,
            "out-of-order chunks are accepted");
  }
  require(smallAssembler.assemble() == honest.snapshot,
          "multi-chunk assembly is byte-identical");

  bool refused = false;
  try {
    SnapshotAssembler oversized(honest.checkpoint.fields().snapshotDigest,
                                parameters().maxSnapshotBytes() + 1,
                                parameters().snapshotChunkBytes(),
                                parameters().maxSnapshotBytes());
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  require(refused, "a transfer claiming more than the cap is refused upfront");

  // A lying size claim makes every chunk fail.
  SnapshotAssembler liar(honest.checkpoint.fields().snapshotDigest,
                         server.totalBytes() + parameters().snapshotChunkBytes(),
                         parameters().snapshotChunkBytes(),
                         parameters().maxSnapshotBytes());
  require(liar.accept(server.chunk(0)) != SnapshotChunkAcceptStatus::ACCEPTED,
          "a wrong chunk count invalidates every proof");
}

} // namespace

int main() {
  try {
    testAnchorParsing();
    HistoryChainFixture chain("bootstrap-tests");
    chain.produce(20);
    testHonestBootstrapReachesTip(chain);
    testMaliciousPeerIsRejected(chain);
    testChunkedSnapshotTransfer(chain);
    std::cout << "Checkpoint bootstrap tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Checkpoint bootstrap tests failed: " << error.what() << "\n";
    return 1;
  }
}
