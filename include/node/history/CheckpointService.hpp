#ifndef NODO_NODE_HISTORY_CHECKPOINT_SERVICE_HPP
#define NODO_NODE_HISTORY_CHECKPOINT_SERVICE_HPP

#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"
#include "node/history/HistoryStore.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class NodeRuntime;

enum class CheckpointCreationStatus {
  CREATED,
  ALREADY_PRESENT,
  NOT_DUE,
  SKIPPED,
  CONFLICT,
  FAILED
};

std::string checkpointCreationStatusToString(CheckpointCreationStatus status);

struct CheckpointCreationResult {
  CheckpointCreationStatus status = CheckpointCreationStatus::FAILED;
  std::string reason;
  std::optional<FinalizedStateCheckpoint> checkpoint;
};

struct CheckpointArtifacts {
  FinalizedStateCheckpoint checkpoint;
  std::vector<unsigned char> snapshotBytes;
  std::vector<archive::ArchiveSegmentCommitment> newSegments;
};

struct CheckpointBackfillResult {
  bool success = false;
  std::uint64_t replayedHeight = 0;
  std::uint64_t createdCheckpoints = 0;
  std::uint64_t existingCheckpoints = 0;
  std::string reason;
};

/*
 * CheckpointService creates finalized state checkpoints from state the node
 * already validated (ADR 0014). It never invents a checkpoint: the height
 * must be final, the QC is the block's own finalization certificate, the
 * snapshot must reproduce the header state root, and every artifact is
 * re-verified before it is written. There is no administrator path that
 * creates a checkpoint from operator input.
 */
class CheckpointService {
public:
  // Builds and self-verifies the checkpoint, its snapshot and any newly
  // sealed archive segment commitments. Throws on any inconsistency.
  static CheckpointArtifacts
  build(const config::GenesisConfig &genesisConfig,
        const config::HistoryParameters &parameters, std::uint64_t height,
        const std::string &blockHash, const std::string &previousBlockHash,
        std::int64_t blockTimestamp, const ProtocolReplayState &replayState,
        const consensus::QuorumCertificate &quorumCertificate,
        const HistoryStore &store,
        const archive::ArchivedBlockSource &blockSource);

  // Segment commitments first, then the snapshot, then the checkpoint, so an
  // existing checkpoint file always implies its snapshot exists.
  static CheckpointCreationResult persist(const HistoryStore &store,
                                          const CheckpointArtifacts &artifacts);

  // Called after a finalized block is durably stored. Never throws; a
  // failure here never invalidates the already final block.
  static CheckpointCreationResult
  onBlockFinalized(const NodeDataDirectoryConfig &directory,
                   const NodeRuntime &runtime);

  // Full replay from genesis over the stored finalized blocks, creating every
  // missing scheduled checkpoint (archive and audit path).
  static CheckpointBackfillResult
  backfill(const NodeDataDirectoryConfig &directory,
           const config::GenesisConfig &genesisConfig);

  // Re-verifies a stored checkpoint: identity, snapshot, QC signatures
  // against the committed set, link to the previous stored checkpoint and,
  // when all segment commitments are local, the archive index root.
  static CheckpointVerificationResult
  verifyStored(const NodeDataDirectoryConfig &directory,
               const config::GenesisConfig &genesisConfig,
               std::uint64_t height);

  static archive::ArchivedBlockSource
  runtimeBlockSource(const NodeRuntime &runtime);
  static archive::ArchivedBlockSource
  blockFileSource(const NodeDataDirectoryConfig &directory);
};

} // namespace nodo::node

#endif
