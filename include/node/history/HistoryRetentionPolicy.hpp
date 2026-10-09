#ifndef NODO_NODE_HISTORY_HISTORY_RETENTION_POLICY_HPP
#define NODO_NODE_HISTORY_HISTORY_RETENTION_POLICY_HPP

#include "config/HistoryParameters.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

// Formal node storage modes (ADR 0014).
//   ARCHIVE  keeps every finalized block, record, QC, checkpoint and snapshot
//            and may participate in Proof of Archival.
//   NORMAL   keeps state, checkpoints, headers and QCs, and a bounded window
//            of full blocks after a verified checkpoint.
//   LIGHT    keeps headers, QCs, checkpoints and proofs with the minimum
//            window; full verification of new blocks is a NORMAL duty.
enum class NodeStorageMode { ARCHIVE, NORMAL, LIGHT };

std::string nodeStorageModeToString(NodeStorageMode mode);
// Accepts the legacy pruning name FULL as NORMAL.
std::optional<NodeStorageMode> nodeStorageModeFromString(const std::string &value);

enum class RetentionCategory {
  GENESIS_AND_IDENTITY,
  CHECKPOINT_COMMITMENTS,
  HEADERS_AND_QCS,
  BLOCK_BODIES,
  CHECKPOINT_SNAPSHOTS,
  LEGACY_FAST_SYNC_SNAPSHOTS,
  VALIDATOR_SET_HISTORY,
  SLASHING_EVIDENCE,
  GOVERNANCE_IN_PROGRESS,
  TREASURY_TIMELOCKS,
  REWARD_SETTLEMENT,
  ARCHIVE_SEGMENTS,
  MEMPOOL_AND_OPERATIONAL
};

std::string retentionCategoryToString(RetentionCategory category);

struct RetentionInputs {
  NodeStorageMode mode = NodeStorageMode::ARCHIVE;
  std::uint64_t tipHeight = 0;
  std::uint64_t retentionBlocks = 0;
  std::uint32_t retainedCheckpointSnapshots = 0;
  // Checkpoints whose id, snapshot and QC re-verified locally.
  std::vector<std::uint64_t> verifiedCheckpointHeights;
  std::vector<std::uint64_t> checkpointSnapshotHeights;
  std::vector<std::uint64_t> legacyFastSyncSnapshotHeights;
  // Creation height of the oldest non-terminal governance proposal and of
  // the oldest approved or queued (timelocked) treasury action.
  std::optional<std::uint64_t> earliestOpenGovernanceHeight;
  std::optional<std::uint64_t> earliestTimelockedTreasuryHeight;
  // Proven distinct replicas per sealed segment, from Proof of Archival.
  // Empty means the archival layer has not measured replication.
  std::vector<std::uint32_t> provenReplicasBySegment;
  // True only when the runtime can reload from a checkpoint base instead of
  // replaying from genesis (roadmap 3.17).
  bool checkpointBaseReloadSupported = false;
};

struct RetentionRule {
  RetentionCategory category = RetentionCategory::GENESIS_AND_IDENTITY;
  bool permanent = false;
  // Data for heights below this may be removed; 0 keeps everything.
  std::uint64_t pruneBelowHeight = 0;
  std::string rule;
};

struct RetentionPlan {
  NodeStorageMode mode = NodeStorageMode::ARCHIVE;
  std::uint64_t tipHeight = 0;
  std::optional<std::uint64_t> baseCheckpointHeight;
  // Block bodies below this height are safe to remove *by every category*;
  // whether they are removed also depends on blockBodyPruningAllowed.
  std::uint64_t blockBodySafeBelowHeight = 0;
  bool blockBodyPruningAllowed = false;
  std::vector<std::string> blockBodyBlockers;
  std::vector<std::uint64_t> checkpointSnapshotsToPrune;
  std::vector<std::uint64_t> legacySnapshotsToPrune;
  std::vector<RetentionRule> rules;
};

/*
 * HistoryRetentionPolicy decides, per data category, what a node may delete
 * (ADR 0014). It never uses a bare "older than X" rule: the block-body floor
 * is the minimum over the retention window, the verified checkpoint base,
 * the reward-settlement epoch, open governance and treasury timelocks and
 * proven archival replication. Pure function, no I/O.
 */
class HistoryRetentionPolicy {
public:
  static RetentionPlan plan(const config::HistoryParameters &parameters,
                            const RetentionInputs &inputs);
};

} // namespace nodo::node

#endif
