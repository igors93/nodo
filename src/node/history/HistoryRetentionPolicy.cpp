#include "node/history/HistoryRetentionPolicy.hpp"

#include "node/AccountabilityWindow.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace nodo::node {

namespace {

RetentionRule permanentRule(RetentionCategory category, std::string rule) {
  return RetentionRule{category, true, 0, std::move(rule)};
}

RetentionRule windowRule(RetentionCategory category, std::uint64_t pruneBelow,
                         std::string rule) {
  return RetentionRule{category, false, pruneBelow, std::move(rule)};
}

std::uint64_t floorBelowWindow(std::uint64_t tip, std::uint64_t window) {
  // Heights > tip - window stay; height 0 is genesis and never a body.
  return tip > window ? tip - window + 1 : 0;
}

} // namespace

std::string nodeStorageModeToString(NodeStorageMode mode) {
  switch (mode) {
  case NodeStorageMode::ARCHIVE:
    return "ARCHIVE";
  case NodeStorageMode::NORMAL:
    return "NORMAL";
  case NodeStorageMode::LIGHT:
    return "LIGHT";
  }
  return "ARCHIVE";
}

std::optional<NodeStorageMode>
nodeStorageModeFromString(const std::string &value) {
  std::string upper = value;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](char c) {
    return static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  });
  if (upper == "ARCHIVE") {
    return NodeStorageMode::ARCHIVE;
  }
  if (upper == "NORMAL" || upper == "FULL") {
    return NodeStorageMode::NORMAL;
  }
  if (upper == "LIGHT") {
    return NodeStorageMode::LIGHT;
  }
  return std::nullopt;
}

std::string retentionCategoryToString(RetentionCategory category) {
  switch (category) {
  case RetentionCategory::GENESIS_AND_IDENTITY:
    return "GENESIS_AND_IDENTITY";
  case RetentionCategory::CHECKPOINT_COMMITMENTS:
    return "CHECKPOINT_COMMITMENTS";
  case RetentionCategory::HEADERS_AND_QCS:
    return "HEADERS_AND_QCS";
  case RetentionCategory::BLOCK_BODIES:
    return "BLOCK_BODIES";
  case RetentionCategory::CHECKPOINT_SNAPSHOTS:
    return "CHECKPOINT_SNAPSHOTS";
  case RetentionCategory::LEGACY_FAST_SYNC_SNAPSHOTS:
    return "LEGACY_FAST_SYNC_SNAPSHOTS";
  case RetentionCategory::VALIDATOR_SET_HISTORY:
    return "VALIDATOR_SET_HISTORY";
  case RetentionCategory::SLASHING_EVIDENCE:
    return "SLASHING_EVIDENCE";
  case RetentionCategory::GOVERNANCE_IN_PROGRESS:
    return "GOVERNANCE_IN_PROGRESS";
  case RetentionCategory::TREASURY_TIMELOCKS:
    return "TREASURY_TIMELOCKS";
  case RetentionCategory::REWARD_SETTLEMENT:
    return "REWARD_SETTLEMENT";
  case RetentionCategory::ARCHIVE_SEGMENTS:
    return "ARCHIVE_SEGMENTS";
  case RetentionCategory::MEMPOOL_AND_OPERATIONAL:
    return "MEMPOOL_AND_OPERATIONAL";
  }
  return "GENESIS_AND_IDENTITY";
}

RetentionPlan HistoryRetentionPolicy::plan(
    const config::HistoryParameters &parameters, const RetentionInputs &in) {
  if (!parameters.isValid()) {
    throw std::invalid_argument("Retention planning needs valid parameters.");
  }
  RetentionPlan plan;
  plan.mode = in.mode;
  plan.tipHeight = in.tipHeight;

  const bool archive = in.mode == NodeStorageMode::ARCHIVE;
  const std::uint64_t retention =
      in.mode == NodeStorageMode::LIGHT
          ? parameters.minimumPruningRetentionBlocks()
          : std::max(in.retentionBlocks,
                     parameters.minimumPruningRetentionBlocks());
  const std::uint32_t keepSnapshots =
      std::max(in.retainedCheckpointSnapshots,
               parameters.minimumRetainedCheckpointSnapshots());

  plan.rules.push_back(permanentRule(
      RetentionCategory::GENESIS_AND_IDENTITY,
      "genesis document, network identity, protocol version and storage "
      "schema are never pruned"));
  plan.rules.push_back(permanentRule(
      RetentionCategory::CHECKPOINT_COMMITMENTS,
      "checkpoint records with their QCs and archive segment commitments are "
      "small and never pruned"));
  plan.rules.push_back(permanentRule(
      RetentionCategory::HEADERS_AND_QCS,
      "finalized headers and QCs stay so light clients and audits can follow "
      "the finality chain"));

  // Verified checkpoint base: newest checkpoint past the confirmation depth
  // whose snapshot is present.
  const std::set<std::uint64_t> snapshots(in.checkpointSnapshotHeights.begin(),
                                          in.checkpointSnapshotHeights.end());
  for (auto it = in.verifiedCheckpointHeights.rbegin();
       it != in.verifiedCheckpointHeights.rend(); ++it) {
    if (*it + parameters.checkpointConfirmationBlocks() <= in.tipHeight &&
        snapshots.count(*it) != 0) {
      plan.baseCheckpointHeight = *it;
      break;
    }
  }

  // Block bodies: the minimum over every category that still needs them.
  std::uint64_t safeBelow = floorBelowWindow(in.tipHeight, retention);
  std::string windowRuleText =
      "keep the last " + std::to_string(retention) + " full blocks";
  plan.rules.push_back(
      windowRule(RetentionCategory::BLOCK_BODIES, safeBelow, windowRuleText));

  const std::uint64_t replayFloor =
      plan.baseCheckpointHeight ? *plan.baseCheckpointHeight + 1 : 0;
  safeBelow = std::min(safeBelow, replayFloor);

  // Epoch reward settlement replays the participation of the whole current
  // epoch from its blocks and finality records.
  const std::uint64_t settlementFloor =
      in.tipHeight == 0
          ? 0
          : ValidatorLifecycle::epochStartBlock(
                ValidatorLifecycle::epochIndexForBlock(in.tipHeight + 1));
  safeBelow = std::min(safeBelow, settlementFloor);
  plan.rules.push_back(windowRule(
      RetentionCategory::REWARD_SETTLEMENT, settlementFloor,
      "keep every block of the epoch whose settlement is still pending"));

  if (in.earliestOpenGovernanceHeight) {
    safeBelow = std::min(safeBelow, *in.earliestOpenGovernanceHeight);
  }
  plan.rules.push_back(windowRule(
      RetentionCategory::GOVERNANCE_IN_PROGRESS,
      in.earliestOpenGovernanceHeight.value_or(safeBelow),
      "keep blocks from the creation of the oldest open proposal"));
  if (in.earliestTimelockedTreasuryHeight) {
    safeBelow = std::min(safeBelow, *in.earliestTimelockedTreasuryHeight);
  }
  plan.rules.push_back(windowRule(
      RetentionCategory::TREASURY_TIMELOCKS,
      in.earliestTimelockedTreasuryHeight.value_or(safeBelow),
      "keep blocks of approved or queued treasury actions until executed"));

  // Archival availability: never drop a segment the archive layer has not
  // proven to be held by enough distinct providers.
  const std::uint32_t minReplicas =
      parameters.archiveMinProvenReplicasBeforePrune();
  if (minReplicas > 0 && safeBelow > 1) {
    const std::uint64_t needed = parameters.segmentIndexForHeight(safeBelow - 1);
    for (std::uint64_t segment = 0; segment <= needed; ++segment) {
      if (segment >= in.provenReplicasBySegment.size() ||
          in.provenReplicasBySegment[segment] < minReplicas) {
        safeBelow =
            std::min(safeBelow, parameters.segmentFirstHeight(segment));
        break;
      }
    }
  }
  plan.rules.push_back(windowRule(
      RetentionCategory::ARCHIVE_SEGMENTS, safeBelow,
      "drop block bodies only after " + std::to_string(minReplicas) +
          " distinct archive providers prove the segment"));

  // Equivocation evidence verification needs validator sets, not bodies;
  // the checkpoint snapshot window carries them.
  plan.rules.push_back(windowRule(
      RetentionCategory::VALIDATOR_SET_HISTORY,
      floorBelowWindow(in.tipHeight + 1,
                       AccountabilityWindow::kEvidenceMaxAgeBlocks),
      "keep validator sets for the whole evidence admission window"));
  plan.rules.push_back(windowRule(
      RetentionCategory::SLASHING_EVIDENCE,
      floorBelowWindow(in.tipHeight + 1,
                       AccountabilityWindow::kEvidenceMaxAgeBlocks),
      "pending evidence is kept until included or older than the evidence "
      "window"));
  plan.rules.push_back(windowRule(
      RetentionCategory::MEMPOOL_AND_OPERATIONAL, 0,
      "mempool and runtime files are operational state, not history"));

  plan.blockBodySafeBelowHeight = archive ? 0 : safeBelow;
  if (archive) {
    plan.blockBodyBlockers.push_back("archive mode keeps every block");
  }
  if (!plan.baseCheckpointHeight) {
    plan.blockBodyBlockers.push_back(
        "no verified checkpoint is past the confirmation depth");
  }
  if (!in.checkpointBaseReloadSupported) {
    plan.blockBodyBlockers.push_back(
        "the runtime still reloads by replaying from genesis; block bodies "
        "stay until checkpoint-base reload lands (roadmap 3.17)");
  }
  if (plan.blockBodySafeBelowHeight <= 1) {
    plan.blockBodyBlockers.push_back("no block is below every retention floor");
  }
  plan.blockBodyPruningAllowed = plan.blockBodyBlockers.empty();

  // Checkpoint snapshots: keep the newest N and the base; archives keep all.
  if (!archive) {
    std::vector<std::uint64_t> ordered(snapshots.begin(), snapshots.end());
    std::sort(ordered.rbegin(), ordered.rend());
    for (std::size_t index = keepSnapshots; index < ordered.size(); ++index) {
      if (!plan.baseCheckpointHeight || ordered[index] != *plan.baseCheckpointHeight) {
        plan.checkpointSnapshotsToPrune.push_back(ordered[index]);
      }
    }
    std::sort(plan.checkpointSnapshotsToPrune.begin(),
              plan.checkpointSnapshotsToPrune.end());

    // Legacy fast-sync snapshots are superseded by checkpoint snapshots and
    // never read by reload; keep only the newest.
    std::vector<std::uint64_t> legacy = in.legacyFastSyncSnapshotHeights;
    std::sort(legacy.begin(), legacy.end());
    if (legacy.size() > 1) {
      plan.legacySnapshotsToPrune.assign(legacy.begin(), legacy.end() - 1);
    }
  }
  plan.rules.push_back(windowRule(
      RetentionCategory::CHECKPOINT_SNAPSHOTS, 0,
      archive ? "archive mode keeps every checkpoint snapshot"
              : "keep the newest " + std::to_string(keepSnapshots) +
                    " checkpoint snapshots and the pruning base"));
  plan.rules.push_back(windowRule(
      RetentionCategory::LEGACY_FAST_SYNC_SNAPSHOTS, 0,
      archive ? "archive mode keeps every legacy snapshot"
              : "keep only the newest legacy fast-sync snapshot"));
  return plan;
}

} // namespace nodo::node
