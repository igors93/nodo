#include "node/NodePruningService.hpp"

#include "node/FastSyncSnapshotStore.hpp"
#include "node/RuntimeStartupService.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "node/history/HistoryPruningEngine.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool parseHeightFromFastSnapshotName(const std::filesystem::path &path,
                                     std::uint64_t &height) {
  if (path.extension() != ".fastsnap")
    return false;
  const std::string text = path.stem().string();
  if (text.empty())
    return false;
  for (char c : text) {
    if (c < '0' || c > '9')
      return false;
  }
  try {
    std::size_t used = 0;
    const unsigned long long parsed = std::stoull(text, &used);
    if (used != text.size())
      return false;
    height = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<std::filesystem::path>
sortedRegularFiles(const std::filesystem::path &directory) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(directory)) {
    return files;
  }
  if (!std::filesystem::is_directory(directory)) {
    throw std::runtime_error("Expected directory path is not a directory: " +
                             directory.string());
  }
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

} // namespace

std::string nodePruningStatusToString(NodePruningStatus status) {
  switch (status) {
  case NodePruningStatus::APPLIED:
    return "APPLIED";
  case NodePruningStatus::NOOP:
    return "NOOP";
  case NodePruningStatus::REJECTED:
    return "REJECTED";
  default:
    return "REJECTED";
  }
}

NodePruningResult::NodePruningResult()
    : m_status(NodePruningStatus::REJECTED),
      m_reason("Uninitialized pruning result."), m_manifest(std::nullopt),
      m_plan(std::nullopt) {}

NodePruningResult NodePruningResult::applied(NodePruningManifest manifest,
                                             NodePruningPlan plan,
                                             std::string message) {
  NodePruningResult result;
  result.m_status = NodePruningStatus::APPLIED;
  result.m_reason = std::move(message);
  result.m_manifest = std::move(manifest);
  result.m_plan = std::move(plan);
  return result;
}

NodePruningResult NodePruningResult::noop(NodePruningManifest manifest,
                                          NodePruningPlan plan,
                                          std::string message) {
  NodePruningResult result;
  result.m_status = NodePruningStatus::NOOP;
  result.m_reason = std::move(message);
  result.m_manifest = std::move(manifest);
  result.m_plan = std::move(plan);
  return result;
}

NodePruningResult NodePruningResult::rejected(std::string reason) {
  NodePruningResult result;
  result.m_status = NodePruningStatus::REJECTED;
  result.m_reason = std::move(reason);
  return result;
}

NodePruningResult NodePruningResult::fromRun(const PruningRunResult &run) {
  NodePruningResult result;
  switch (run.status) {
  case PruningRunStatus::APPLIED:
    result.m_status = NodePruningStatus::APPLIED;
    break;
  case PruningRunStatus::NOOP:
  case PruningRunStatus::DRY_RUN:
    result.m_status = NodePruningStatus::NOOP;
    break;
  case PruningRunStatus::REJECTED:
    result.m_status = NodePruningStatus::REJECTED;
    break;
  }
  result.m_reason = run.reason;
  result.m_run = run;
  return result;
}

const std::optional<PruningRunResult> &NodePruningResult::run() const {
  return m_run;
}

NodePruningStatus NodePruningResult::status() const { return m_status; }
const std::string &NodePruningResult::reason() const { return m_reason; }
bool NodePruningResult::success() const {
  return m_status != NodePruningStatus::REJECTED;
}
bool NodePruningResult::applied() const {
  return m_status == NodePruningStatus::APPLIED;
}
const std::optional<NodePruningManifest> &NodePruningResult::manifest() const {
  return m_manifest;
}
const std::optional<NodePruningPlan> &NodePruningResult::plan() const {
  return m_plan;
}

std::string NodePruningResult::serialize() const {
  std::ostringstream oss;
  oss << "NodePruningResult{"
      << "status=" << nodePruningStatusToString(m_status)
      << ";reason=" << m_reason << ";manifest="
      << (m_manifest.has_value() ? m_manifest->serialize() : "NONE")
      << ";plan=" << (m_plan.has_value() ? m_plan->serialize() : "NONE") << "}";
  return oss.str();
}

std::optional<NodePruningManifest> NodePruningService::loadManifest(
    const NodeDataDirectoryConfig &directoryConfig) {
  if (!directoryConfig.isValid() ||
      !std::filesystem::exists(directoryConfig.pruningManifestPath())) {
    return std::nullopt;
  }
  try {
    return NodePruningManifest::fromFileContents(
        storage::AtomicFile::readTextFile(
            directoryConfig.pruningManifestPath()));
  } catch (...) {
    return std::nullopt;
  }
}

NodePruningPlan
NodePruningService::buildPlan(const NodeDataDirectoryConfig &directoryConfig,
                              const NodeRuntimeManifest &runtimeManifest,
                              const NodePruningConfig &pruningConfig) {
  if (!directoryConfig.isValid() || !runtimeManifest.isValid() ||
      !pruningConfig.isValid()) {
    return NodePruningPlan(pruningConfig, runtimeManifest.latestBlockHeight(),
                           0, 0, "", {}, {}, false,
                           "invalid pruning plan input");
  }

  const std::uint64_t currentHeight = runtimeManifest.latestBlockHeight();
  const std::uint64_t retainFromHeight =
      pruningConfig.retainFromHeight(currentHeight);

  if (pruningConfig.mode() == NodePruningMode::ARCHIVE) {
    return NodePruningPlan(
        pruningConfig, currentHeight, 0, 0, "", {}, {}, true,
        "archive mode keeps all finalized artifacts and snapshots");
  }

  const FastSyncSnapshotStore snapshotStore(
      directoryConfig.fastSyncSnapshotsDirectoryPath());

  const std::uint64_t requiredSnapshotHeight =
      pruningConfig.mode() == NodePruningMode::LIGHT ? currentHeight
                                                     : retainFromHeight;

  const std::optional<FastSyncSnapshot> boundarySnapshot =
      snapshotStore.load(requiredSnapshotHeight);

  if (!boundarySnapshot.has_value() || !boundarySnapshot->isValid()) {
    return NodePruningPlan(pruningConfig, currentHeight, retainFromHeight,
                           requiredSnapshotHeight, "", {}, {}, false,
                           "required fast-sync snapshot boundary is missing or "
                           "invalid at height " +
                               std::to_string(requiredSnapshotHeight));
  }

  // Finalized block files are never listed: reload replays from genesis and
  // needs every one of them. Block-body pruning belongs to
  // HistoryPruningEngine and stays blocked until checkpoint-base reload
  // exists (ADR 0014, roadmap 3.17).
  std::vector<std::filesystem::path> blockArtifactsToPrune;

  std::vector<std::filesystem::path> snapshotsToPrune;
  for (const auto &path :
       sortedRegularFiles(directoryConfig.fastSyncSnapshotsDirectoryPath())) {
    std::uint64_t height = 0;
    if (!parseHeightFromFastSnapshotName(path, height)) {
      continue;
    }
    if (height < retainFromHeight && height != requiredSnapshotHeight) {
      snapshotsToPrune.push_back(path);
    }
  }

  return NodePruningPlan(
      pruningConfig, currentHeight, retainFromHeight, requiredSnapshotHeight,
      boundarySnapshot->digest(), std::move(blockArtifactsToPrune),
      std::move(snapshotsToPrune), true,
      "pruning plan is safe: required fast-sync snapshot boundary is present");
}

NodePruningResult
NodePruningService::apply(const NodeDataDirectoryConfig &directoryConfig,
                          const NodeRuntimeManifest &runtimeManifest,
                          const NodePruningConfig &pruningConfig,
                          std::int64_t now) {
  if (now <= 0) {
    return NodePruningResult::rejected("pruning timestamp must be positive");
  }
  if (!directoryConfig.isValid() || !runtimeManifest.isValid() ||
      !pruningConfig.isValid()) {
    return NodePruningResult::rejected("invalid pruning input");
  }
  const config::GenesisLookupResult genesis =
      RuntimeStartupService::resolveGenesis(runtimeManifest.networkName(),
                                            directoryConfig.rootPath(), {});
  if (!genesis.found()) {
    return NodePruningResult::rejected("cannot resolve genesis: " +
                                       genesis.reason());
  }
  PruningOptions options;
  switch (pruningConfig.mode()) {
  case NodePruningMode::ARCHIVE:
    options.mode = NodeStorageMode::ARCHIVE;
    break;
  case NodePruningMode::FULL:
    options.mode = NodeStorageMode::NORMAL;
    options.retentionBlocks =
        static_cast<std::uint64_t>(pruningConfig.retainEpochs()) *
        NODO_VALIDATOR_EPOCH_BLOCKS;
    break;
  case NodePruningMode::LIGHT:
    options.mode = NodeStorageMode::LIGHT;
    break;
  }
  return NodePruningResult::fromRun(HistoryPruningEngine::run(
      directoryConfig, genesis.genesis(), options, now));
}

NodePruningResult NodePruningService::applyConfiguredPolicy(
    const NodeDataDirectoryConfig &directoryConfig,
    const NodeRuntimeManifest &runtimeManifest, std::int64_t now) {
  if (loadManifest(directoryConfig).has_value()) {
    // A legacy policy could delete block files; it is never re-applied.
    return NodePruningResult::noop(
        NodePruningManifest::archive(runtimeManifest, now), NodePruningPlan(),
        "legacy pruning manifest present; run `nodo storage migrate`");
  }
  std::optional<PruningManifestV2> configured;
  try {
    configured = HistoryPruningEngine::loadManifest(directoryConfig);
  } catch (const std::exception &error) {
    return NodePruningResult::rejected(
        std::string("pruning manifest is corrupt: ") + error.what());
  }
  if (!configured.has_value() || configured->mode == NodeStorageMode::ARCHIVE) {
    return NodePruningResult::noop(
        NodePruningManifest::archive(runtimeManifest, now), NodePruningPlan(),
        "archive mode keeps all finalized history");
  }
  try {
    const config::HistoryParameters parameters =
        config::HistoryParameters::forNetwork(runtimeManifest.networkName());
    if (!HistoryPruningEngine::shouldRunAutomatically(
            parameters, runtimeManifest.latestBlockHeight())) {
      return NodePruningResult::noop(
          NodePruningManifest::archive(runtimeManifest, now),
          NodePruningPlan(), "no newly confirmed checkpoint");
    }
  } catch (const std::exception &error) {
    return NodePruningResult::noop(
        NodePruningManifest::archive(runtimeManifest, now), NodePruningPlan(),
        error.what());
  }
  const config::GenesisLookupResult genesis =
      RuntimeStartupService::resolveGenesis(runtimeManifest.networkName(),
                                            directoryConfig.rootPath(), {});
  if (!genesis.found()) {
    return NodePruningResult::rejected("cannot resolve genesis: " +
                                       genesis.reason());
  }
  PruningOptions options;
  options.mode = configured->mode;
  options.retentionBlocks = configured->retentionBlocks;
  options.retainedCheckpointSnapshots = configured->retainedCheckpointSnapshots;
  return NodePruningResult::fromRun(HistoryPruningEngine::run(
      directoryConfig, genesis.genesis(), options, now));
}

bool NodePruningService::validateManifestAgainstRuntime(
    const NodeDataDirectoryConfig &directoryConfig,
    const NodeRuntimeManifest &runtimeManifest, std::string &reason) {
  std::optional<PruningManifestV2> history;
  try {
    history = HistoryPruningEngine::loadManifest(directoryConfig);
  } catch (const std::exception &error) {
    reason = std::string("pruning manifest is corrupt: ") + error.what();
    return false;
  }
  if (history.has_value()) {
    if (history->chainId != runtimeManifest.chainId() ||
        history->genesisConfigId != runtimeManifest.genesisConfigId()) {
      reason = "pruning manifest does not match runtime chain/genesis";
      return false;
    }
    if (history->lastFinalizedHeight > runtimeManifest.latestBlockHeight()) {
      reason = "pruning manifest is ahead of the runtime manifest";
      return false;
    }
  }

  const std::optional<NodePruningManifest> manifest =
      loadManifest(directoryConfig);
  if (!manifest.has_value()) {
    return true;
  }

  if (manifest->chainId() != runtimeManifest.chainId() ||
      manifest->genesisConfigId() != runtimeManifest.genesisConfigId()) {
    reason = "pruning manifest does not match runtime chain/genesis";
    return false;
  }

  if (manifest->latestHeight() > runtimeManifest.latestBlockHeight()) {
    reason = "pruning manifest is ahead of the runtime manifest";
    return false;
  }

  if (manifest->config().mode() == NodePruningMode::ARCHIVE) {
    return true;
  }

  const FastSyncSnapshotStore snapshotStore(
      directoryConfig.fastSyncSnapshotsDirectoryPath());
  const std::optional<FastSyncSnapshot> snapshot =
      snapshotStore.load(manifest->snapshotBoundaryHeight());
  if (!snapshot.has_value() || !snapshot->isValid()) {
    reason =
        "pruning manifest references a missing or invalid snapshot boundary";
    return false;
  }

  if (snapshot->digest() != manifest->snapshotBoundaryDigest()) {
    reason = "snapshot boundary digest does not match pruning manifest";
    return false;
  }

  return true;
}

} // namespace nodo::node
