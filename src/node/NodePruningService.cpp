#include "node/NodePruningService.hpp"

#include "node/FastSyncSnapshotStore.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool parseHeightFromBlockArtifactName(const std::filesystem::path &path,
                                      std::uint64_t &height) {
  const std::string name = path.filename().string();
  const std::string prefix = "block_";
  if (name.rfind(prefix, 0) != 0 || path.extension() != ".nodo") {
    return false;
  }
  const std::size_t heightStart = prefix.size();
  const std::size_t separator = name.find('_', heightStart);
  if (separator == std::string::npos || separator == heightStart) {
    return false;
  }
  const std::string text = name.substr(heightStart, separator - heightStart);
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

void writePrunedBlockTombstone(const NodeDataDirectoryConfig &directoryConfig,
                               const std::filesystem::path &originalPath,
                               std::uint64_t height) {
  std::filesystem::create_directories(
      directoryConfig.prunedBlocksDirectoryPath());
  std::ostringstream contents;
  contents << "NODO_PRUNED_BLOCK_ARTIFACT_V1\n"
           << "height=" << height << "\n"
           << "originalPath=" << originalPath.filename().string() << "\n";
  storage::AtomicFile::writeTextFile(
      directoryConfig.prunedBlocksDirectoryPath() /
          (std::to_string(height) + ".pruned"),
      contents.str());
}

std::uint64_t
existingPrunedBlockCount(const NodeDataDirectoryConfig &directoryConfig) {
  if (!std::filesystem::exists(directoryConfig.prunedBlocksDirectoryPath())) {
    return 0;
  }
  std::uint64_t count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(
           directoryConfig.prunedBlocksDirectoryPath())) {
    if (entry.is_regular_file() && entry.path().extension() == ".pruned") {
      ++count;
    }
  }
  return count;
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

  std::vector<std::filesystem::path> blockArtifactsToPrune;
  if (pruningConfig.mode() == NodePruningMode::LIGHT) {
    for (const auto &path :
         sortedRegularFiles(directoryConfig.blocksDirectoryPath())) {
      std::uint64_t height = 0;
      if (parseHeightFromBlockArtifactName(path, height) && height > 0 &&
          height < retainFromHeight) {
        blockArtifactsToPrune.push_back(path);
      }
    }
  }

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

  const NodePruningPlan plan =
      buildPlan(directoryConfig, runtimeManifest, pruningConfig);
  if (!plan.safeToApply()) {
    return NodePruningResult::rejected(plan.reason());
  }

  try {
    std::uint64_t prunedBlocks = existingPrunedBlockCount(directoryConfig);
    std::uint64_t prunedSnapshots = 0;

    for (const auto &path : plan.blockArtifactsToPrune()) {
      std::uint64_t height = 0;
      if (!parseHeightFromBlockArtifactName(path, height)) {
        continue;
      }
      writePrunedBlockTombstone(directoryConfig, path, height);
      std::error_code ec;
      std::filesystem::remove(path, ec);
      if (ec) {
        return NodePruningResult::rejected(
            "failed to remove finalized block artifact " + path.string() +
            ": " + ec.message());
      }
      ++prunedBlocks;
    }

    for (const auto &path : plan.snapshotsToPrune()) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
      if (ec) {
        return NodePruningResult::rejected(
            "failed to remove old fast-sync snapshot " + path.string() + ": " +
            ec.message());
      }
      ++prunedSnapshots;
    }

    NodePruningManifest manifest(
        pruningConfig, runtimeManifest.chainId(),
        runtimeManifest.genesisConfigId(), runtimeManifest.latestBlockHeight(),
        runtimeManifest.latestBlockHash(), runtimeManifest.latestStateRoot(),
        plan.retainFromHeight(), plan.snapshotBoundaryHeight(),
        plan.snapshotBoundaryDigest(), prunedBlocks, prunedSnapshots, now);

    if (pruningConfig.mode() == NodePruningMode::ARCHIVE) {
      manifest = NodePruningManifest::archive(runtimeManifest, now);
    }

    if (!manifest.isValid()) {
      return NodePruningResult::rejected(
          "generated pruning manifest is invalid");
    }

    storage::AtomicFile::writeTextFile(directoryConfig.pruningManifestPath(),
                                       manifest.toFileContents());

    if (plan.totalFileCount() == 0) {
      return NodePruningResult::noop(
          std::move(manifest), plan,
          "pruning policy recorded; no files were eligible for removal");
    }

    return NodePruningResult::applied(std::move(manifest), plan,
                                      "pruning policy applied");
  } catch (const std::exception &error) {
    return NodePruningResult::rejected(error.what());
  }
}

NodePruningResult NodePruningService::applyConfiguredPolicy(
    const NodeDataDirectoryConfig &directoryConfig,
    const NodeRuntimeManifest &runtimeManifest, std::int64_t now) {
  const std::optional<NodePruningManifest> manifest =
      loadManifest(directoryConfig);
  if (!manifest.has_value()) {
    return NodePruningResult::noop(
        NodePruningManifest::archive(runtimeManifest, now),
        buildPlan(directoryConfig, runtimeManifest,
                  NodePruningConfig::archiveMode()),
        "no pruning manifest configured; archive mode assumed");
  }
  return apply(directoryConfig, runtimeManifest, manifest->config(), now);
}

bool NodePruningService::validateManifestAgainstRuntime(
    const NodeDataDirectoryConfig &directoryConfig,
    const NodeRuntimeManifest &runtimeManifest, std::string &reason) {
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
