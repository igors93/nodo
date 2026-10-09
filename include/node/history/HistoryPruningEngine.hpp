#ifndef NODO_NODE_HISTORY_HISTORY_PRUNING_ENGINE_HPP
#define NODO_NODE_HISTORY_HISTORY_PRUNING_ENGINE_HPP

#include "config/NetworkParameters.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/history/HistoryRetentionPolicy.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

/*
 * Durable pruning boundary (history/pruning/manifest.nodo). It only ever
 * moves forward after the files a run removes are gone, so a restart never
 * believes an interrupted run completed.
 */
struct PruningManifestV2 {
  static constexpr const char *SCHEMA = "NODO_PRUNING_MANIFEST_V2";

  NodeStorageMode mode = NodeStorageMode::ARCHIVE;
  std::string chainId;
  std::string genesisConfigId;
  std::uint64_t lastFinalizedHeight = 0;
  std::uint64_t lastCheckpointHeight = 0;
  std::string lastCheckpointId;
  // Block bodies below this height were removed; 0 means none ever were.
  std::uint64_t lastPrunedHeight = 0;
  std::uint64_t retentionBlocks = 0;
  std::uint32_t retainedCheckpointSnapshots = 0;
  std::uint64_t prunedBlockBodies = 0;
  std::uint64_t prunedCheckpointSnapshots = 0;
  std::uint64_t prunedLegacySnapshots = 0;
  std::string lastPlanId;
  std::int64_t updatedAt = 0;

  bool isValid() const;
  std::vector<std::pair<std::string, std::string>> fields() const;
  static PruningManifestV2
  fromFields(const std::map<std::string, std::string> &fields,
             const std::string &prefix);
  std::string toFileContents() const;
  static PruningManifestV2 fromFileContents(const std::string &contents);
  std::string serializeJson() const;
};

// Test-only crash simulation points for the two-phase protocol.
enum class PruningCrashPoint {
  NONE,
  AFTER_PREPARE,
  AFTER_FIRST_DELETE,
  BEFORE_COMMIT
};

struct PruningOptions {
  NodeStorageMode mode = NodeStorageMode::NORMAL;
  std::uint64_t retentionBlocks = 0;
  std::uint32_t retainedCheckpointSnapshots = 0;
  bool dryRun = false;
  PruningCrashPoint crashPoint = PruningCrashPoint::NONE;
};

enum class PruningRunStatus { APPLIED, NOOP, DRY_RUN, REJECTED };

std::string pruningRunStatusToString(PruningRunStatus status);

struct PruningRunResult {
  PruningRunStatus status = PruningRunStatus::REJECTED;
  std::string reason;
  RetentionPlan plan;
  std::optional<PruningManifestV2> manifest;
  std::vector<std::string> removedTargets;
  bool recoveredInterruptedRun = false;

  bool success() const { return status != PruningRunStatus::REJECTED; }
};

enum class PruningRecoveryStatus { NOTHING_TO_RECOVER, RECOVERED, QUARANTINED };

struct PruningRecoveryResult {
  PruningRecoveryStatus status = PruningRecoveryStatus::NOTHING_TO_RECOVER;
  std::string reason;
  std::size_t completedDeletes = 0;
};

/*
 * HistoryPruningEngine applies HistoryRetentionPolicy to a data directory
 * with a crash-safe two-phase protocol:
 *
 *   PREPARE  write journal.nodo: plan id, every target, the manifest to commit
 *   DELETE   remove each target (idempotent)
 *   COMMIT   atomically write the manifest, then remove the journal
 *
 * Recovery rolls an interrupted run forward: removals are only ever planned
 * for data every retention rule already released, and that never becomes
 * unsafe as the chain grows. A journal that fails strict parsing or names a
 * path outside the allow-list is quarantined and nothing is deleted.
 */
class HistoryPruningEngine {
public:
  static PruningRunResult run(const NodeDataDirectoryConfig &directory,
                              const config::GenesisConfig &genesisConfig,
                              const PruningOptions &options, std::int64_t now);

  static PruningRecoveryResult recover(const NodeDataDirectoryConfig &directory);

  // Plans without touching anything (status and dry runs).
  static PruningRunResult plan(const NodeDataDirectoryConfig &directory,
                               const config::GenesisConfig &genesisConfig,
                               const PruningOptions &options);

  // nullopt when absent; throws on a corrupt manifest.
  static std::optional<PruningManifestV2>
  loadManifest(const NodeDataDirectoryConfig &directory);

  // Converts runtime/pruning_manifest.nodo (legacy V1) into the V2 manifest
  // during storage migration. Returns a description of the action taken.
  static std::string
  migrateLegacyManifest(const NodeDataDirectoryConfig &directory);

  // Automatic pruning runs once per checkpoint, when the newest checkpoint
  // passes the confirmation depth.
  static bool shouldRunAutomatically(const config::HistoryParameters &parameters,
                                     std::uint64_t tipHeight);

  // Only these relative paths may ever be removed.
  static bool isAllowedTarget(const std::filesystem::path &relative);
};

} // namespace nodo::node

#endif
