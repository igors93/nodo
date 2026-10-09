#include "node/history/HistoryPruningEngine.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "node/NodePruningManifest.hpp"
#include "node/ProtocolDomainCodec.hpp"
#include "node/history/CheckpointService.hpp"
#include "node/history/HistoryStore.hpp"
#include "node/history/StorageMigration.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/JsonText.hpp"
#include "utils/Logger.hpp"
#include "utils/SafeScalar.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nodo::node {

namespace {

constexpr const char *kJournalSchema = "NODO_PRUNING_JOURNAL_V1";
constexpr const char *kNone = "NONE";
constexpr std::size_t kMaxJournalTargets = 1'000'000;

std::string orNone(const std::string &value) {
  return value.empty() ? kNone : value;
}

std::string fromNone(const std::string &value) {
  return value == kNone ? "" : value;
}

std::uint64_t parseU64(const std::map<std::string, std::string> &fields,
                       const std::string &key) {
  const auto found = fields.find(key);
  if (found == fields.end() || found->second.empty() ||
      found->second.size() > 20 ||
      found->second.find_first_not_of("0123456789") != std::string::npos ||
      (found->second.size() > 1 && found->second.front() == '0')) {
    throw std::invalid_argument("Malformed pruning numeric field: " + key);
  }
  return std::stoull(found->second);
}

std::int64_t parseI64(const std::map<std::string, std::string> &fields,
                      const std::string &key) {
  const std::uint64_t value = parseU64(fields, key);
  if (value > static_cast<std::uint64_t>(
                  std::numeric_limits<std::int64_t>::max())) {
    throw std::invalid_argument("Pruning timestamp is out of range: " + key);
  }
  return static_cast<std::int64_t>(value);
}

std::string requireField(const std::map<std::string, std::string> &fields,
                         const std::string &key) {
  const auto found = fields.find(key);
  if (found == fields.end()) {
    throw std::invalid_argument("Missing pruning field: " + key);
  }
  return found->second;
}

bool isNumbered(const std::string &name, const std::string &prefix,
                const std::string &suffix) {
  if (name.size() <= prefix.size() + suffix.size() ||
      name.compare(0, prefix.size(), prefix) != 0 ||
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
    return false;
  }
  const std::string digits =
      name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
  return digits.size() <= 20 &&
         digits.find_first_not_of("0123456789") == std::string::npos &&
         (digits.size() == 1 || digits.front() != '0');
}

std::vector<std::uint64_t>
legacySnapshotHeights(const NodeDataDirectoryConfig &directory) {
  std::vector<std::uint64_t> heights;
  std::error_code ec;
  const auto path = directory.fastSyncSnapshotsDirectoryPath();
  if (!std::filesystem::is_directory(path, ec)) {
    return heights;
  }
  for (const auto &entry : std::filesystem::directory_iterator(path)) {
    if (!entry.is_regular_file() || entry.is_symlink()) {
      continue;
    }
    const auto height =
        HistoryStore::parseNumberedFileName(entry.path(), ".fastsnap");
    if (height) {
      heights.push_back(*height);
    }
  }
  std::sort(heights.begin(), heights.end());
  return heights;
}

std::filesystem::path relativeSnapshot(std::uint64_t height) {
  return std::filesystem::path("history") / "snapshots" /
         (std::to_string(height) + ".snapshot");
}

std::filesystem::path relativeLegacySnapshot(std::uint64_t height) {
  return std::filesystem::path("runtime") / "fast_sync_snapshots" /
         (std::to_string(height) + ".fastsnap");
}

struct Journal {
  std::string planId;
  std::vector<std::filesystem::path> targets;
  PruningManifestV2 manifest;
};

std::string planIdFor(const std::vector<std::filesystem::path> &targets,
                      const PruningManifestV2 &manifest) {
  serialization::CanonicalWriter writer;
  writer.writeUInt32(static_cast<std::uint32_t>(targets.size()));
  for (const auto &target : targets) {
    writer.writeString(target.generic_string());
  }
  for (const auto &[key, value] : manifest.fields()) {
    if (key == "lastPlanId") {
      continue;
    }
    writer.writeString(key);
    writer.writeString(value);
  }
  return archive::hashHex("HISTORY/PRUNING-PLAN", writer.bytes());
}

std::string encodeJournal(const Journal &journal) {
  std::vector<std::pair<std::string, std::string>> fields;
  fields.emplace_back("planId", journal.planId);
  fields.emplace_back("targetCount", std::to_string(journal.targets.size()));
  for (std::size_t index = 0; index < journal.targets.size(); ++index) {
    fields.emplace_back("target." + std::to_string(index),
                        journal.targets[index].generic_string());
  }
  for (const auto &[key, value] : journal.manifest.fields()) {
    fields.emplace_back("manifest." + key, value);
  }
  return serialization::KeyValueFileCodec::serialize(kJournalSchema, fields);
}

Journal decodeJournal(const std::string &contents) {
  const serialization::KeyValueFileDocument document =
      serialization::KeyValueFileCodec::parse(contents, kJournalSchema);
  const std::map<std::string, std::string> &fields = document.fields();
  Journal journal;
  journal.planId = requireField(fields, "planId");
  const std::uint64_t count = parseU64(fields, "targetCount");
  if (count > kMaxJournalTargets) {
    throw std::invalid_argument("Pruning journal has too many targets.");
  }
  std::set<std::string> allowed = {"planId", "targetCount"};
  for (std::uint64_t index = 0; index < count; ++index) {
    const std::string key = "target." + std::to_string(index);
    allowed.insert(key);
    const std::filesystem::path target(requireField(fields, key));
    if (!HistoryPruningEngine::isAllowedTarget(target)) {
      throw std::invalid_argument("Pruning journal names a forbidden path.");
    }
    journal.targets.push_back(target);
  }
  journal.manifest = PruningManifestV2::fromFields(fields, "manifest.");
  for (const auto &[key, value] : journal.manifest.fields()) {
    (void)value;
    allowed.insert("manifest." + key);
  }
  document.requireOnlyFields(allowed);
  if (!archive::isDigestHex(journal.planId) ||
      journal.manifest.lastPlanId != journal.planId ||
      planIdFor(journal.targets, journal.manifest) != journal.planId ||
      encodeJournal(journal) != contents) {
    throw std::invalid_argument("Pruning journal is not self-consistent.");
  }
  return journal;
}

// Removes one allow-listed target. Missing files are already done.
void removeTarget(const NodeDataDirectoryConfig &directory,
                  const std::filesystem::path &relative) {
  if (!HistoryPruningEngine::isAllowedTarget(relative)) {
    throw std::runtime_error("Refusing to remove a non-allow-listed path.");
  }
  const std::filesystem::path path = directory.rootPath() / relative;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || status.type() == std::filesystem::file_type::not_found) {
    return;
  }
  if (status.type() != std::filesystem::file_type::regular) {
    throw std::runtime_error("Pruning target is not a regular file: " +
                             relative.generic_string());
  }
  std::filesystem::remove(path, ec);
  if (ec) {
    throw std::runtime_error("Failed to remove " + relative.generic_string() +
                             ": " + ec.message());
  }
}

void commitManifest(const NodeDataDirectoryConfig &directory,
                    const PruningManifestV2 &manifest) {
  std::filesystem::create_directories(directory.historyPruningDirectoryPath());
  storage::AtomicFile::writeTextFile(directory.historyPruningManifestPath(),
                                     manifest.toFileContents());
}

std::optional<std::uint64_t>
earliestOpenProposal(const GovernanceExecutor &governance, bool treasuryOnly) {
  std::optional<std::uint64_t> earliest;
  for (const std::string &id : governance.proposalIds()) {
    const auto proposal = governance.proposalSnapshot(id);
    const bool open = proposal.status == GovernanceProposalStatus::PENDING ||
                      proposal.status == GovernanceProposalStatus::ACTIVE ||
                      proposal.status == GovernanceProposalStatus::APPROVED ||
                      proposal.status ==
                          GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
    const bool timelocked =
        proposal.status == GovernanceProposalStatus::APPROVED ||
        proposal.status == GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
    if ((treasuryOnly ? timelocked : open) && proposal.createdHeight > 0) {
      earliest = earliest ? std::min(*earliest, proposal.createdHeight)
                          : proposal.createdHeight;
    }
  }
  return earliest;
}

PruningRunResult rejectedRun(std::string reason) {
  PruningRunResult result;
  result.status = PruningRunStatus::REJECTED;
  result.reason = std::move(reason);
  return result;
}

struct PlannedRun {
  PruningRunResult result;
  std::vector<std::filesystem::path> targets;
  PruningManifestV2 manifest;
};

PlannedRun planRun(const NodeDataDirectoryConfig &directory,
                   const config::GenesisConfig &genesisConfig,
                   const PruningOptions &options, std::int64_t now) {
  PlannedRun planned;
  if (!StorageMigration::supportsHistoryLayout(directory)) {
    planned.result = rejectedRun("storage schema predates the history layout; "
                                 "run `nodo storage migrate` first");
    return planned;
  }
  const NodeDataDirectoryReadResult runtime =
      NodeDataDirectory::loadManifest(directory);
  if (!runtime.loaded() ||
      runtime.manifest().genesisConfigId() != genesisConfig.deterministicId()) {
    planned.result =
        rejectedRun("data directory manifest is missing or for another genesis");
    return planned;
  }
  const config::HistoryParameters parameters =
      config::HistoryParameters::forNetwork(
          genesisConfig.networkParameters().networkName());
  const HistoryStore store(directory);
  const std::uint64_t tip = runtime.manifest().latestBlockHeight();

  RetentionInputs inputs;
  inputs.mode = options.mode;
  inputs.tipHeight = tip;
  inputs.retentionBlocks = options.retentionBlocks;
  inputs.retainedCheckpointSnapshots = options.retainedCheckpointSnapshots;
  inputs.checkpointSnapshotHeights = store.snapshotHeights();
  inputs.legacyFastSyncSnapshotHeights = legacySnapshotHeights(directory);

  // Only the base candidate is re-verified: newest checkpoint past the
  // confirmation depth whose snapshot, id and QC check out.
  const std::vector<std::uint64_t> checkpoints = store.checkpointHeights();
  for (auto it = checkpoints.rbegin(); it != checkpoints.rend(); ++it) {
    if (*it + parameters.checkpointConfirmationBlocks() > tip) {
      continue;
    }
    if (CheckpointService::verifyStored(directory, genesisConfig, *it)
            .isAccepted()) {
      inputs.verifiedCheckpointHeights.push_back(*it);
      break;
    }
  }

  // Governance and treasury floors from the newest checkpoint snapshot.
  // Proposals opened later are above every candidate floor, and proposals
  // closed since then are kept conservatively.
  if (!checkpoints.empty()) {
    try {
      const auto latest =
          store.loadSnapshot(checkpoints.back(), parameters.maxSnapshotBytes());
      if (latest) {
        const ProtocolExecutionState execution =
            ProtocolDomainCodec::decodeState(latest->state().protocolDomains());
        inputs.earliestOpenGovernanceHeight =
            earliestOpenProposal(execution.governance, false);
        inputs.earliestTimelockedTreasuryHeight =
            earliestOpenProposal(execution.governance, true);
      }
    } catch (const std::exception &) {
      // Unknown governance state forbids body pruning below the snapshot.
      inputs.earliestOpenGovernanceHeight = checkpoints.back();
    }
  }

  planned.result.plan = HistoryRetentionPolicy::plan(parameters, inputs);
  const RetentionPlan &plan = planned.result.plan;
  for (const std::uint64_t height : plan.checkpointSnapshotsToPrune) {
    planned.targets.push_back(relativeSnapshot(height));
  }
  for (const std::uint64_t height : plan.legacySnapshotsToPrune) {
    planned.targets.push_back(relativeLegacySnapshot(height));
  }
  // Block bodies are removed only when every rule and the runtime allow it.
  if (plan.blockBodyPruningAllowed) {
    std::error_code ec;
    for (const auto &entry :
         std::filesystem::directory_iterator(directory.blocksDirectoryPath(), ec)) {
      const std::string name = entry.path().filename().string();
      const std::size_t separator = name.find('_', 6);
      if (name.rfind("block_", 0) != 0 || separator == std::string::npos) {
        continue;
      }
      const auto height = HistoryStore::parseNumberedFileName(
          std::filesystem::path(name.substr(6, separator - 6) + ".nodo"),
          ".nodo");
      if (height && *height > 0 && *height < plan.blockBodySafeBelowHeight) {
        planned.targets.push_back(std::filesystem::path("blocks") / name);
      }
    }
  }
  std::sort(planned.targets.begin(), planned.targets.end());

  std::optional<PruningManifestV2> previous;
  try {
    previous = HistoryPruningEngine::loadManifest(directory);
  } catch (const std::exception &error) {
    planned.result = rejectedRun(std::string("pruning manifest is corrupt: ") +
                                 error.what());
    return planned;
  }
  PruningManifestV2 &manifest = planned.manifest;
  manifest.mode = options.mode;
  manifest.chainId = runtime.manifest().chainId();
  manifest.genesisConfigId = runtime.manifest().genesisConfigId();
  manifest.lastFinalizedHeight = tip;
  if (!checkpoints.empty()) {
    manifest.lastCheckpointHeight = checkpoints.back();
    if (const auto latest = store.loadCheckpoint(checkpoints.back())) {
      manifest.lastCheckpointId = latest->checkpointId();
    }
  }
  manifest.retentionBlocks =
      std::max(options.retentionBlocks, parameters.minimumPruningRetentionBlocks());
  manifest.retainedCheckpointSnapshots =
      std::max(options.retainedCheckpointSnapshots,
               parameters.minimumRetainedCheckpointSnapshots());
  manifest.lastPrunedHeight = previous ? previous->lastPrunedHeight : 0;
  manifest.prunedBlockBodies = previous ? previous->prunedBlockBodies : 0;
  manifest.prunedCheckpointSnapshots =
      (previous ? previous->prunedCheckpointSnapshots : 0) +
      plan.checkpointSnapshotsToPrune.size();
  manifest.prunedLegacySnapshots =
      (previous ? previous->prunedLegacySnapshots : 0) +
      plan.legacySnapshotsToPrune.size();
  if (plan.blockBodyPruningAllowed) {
    const std::size_t bodies = std::count_if(
        planned.targets.begin(), planned.targets.end(),
        [](const std::filesystem::path &p) {
          return p.begin() != p.end() && *p.begin() == "blocks";
        });
    manifest.prunedBlockBodies += bodies;
    manifest.lastPrunedHeight =
        std::max(manifest.lastPrunedHeight, plan.blockBodySafeBelowHeight);
  }
  manifest.updatedAt = now;
  manifest.lastPlanId = planIdFor(planned.targets, manifest);
  planned.result.status = PruningRunStatus::DRY_RUN;
  planned.result.manifest = manifest;
  for (const auto &target : planned.targets) {
    planned.result.removedTargets.push_back(target.generic_string());
  }
  return planned;
}

} // namespace

bool PruningManifestV2::isValid() const {
  return utils::isSafeIdentifier(chainId, 128, "_-.:") &&
         utils::isSafeIdentifier(genesisConfigId, 128, "_-.:") &&
         lastCheckpointHeight <= lastFinalizedHeight &&
         lastPrunedHeight <= lastFinalizedHeight + 1 &&
         (lastCheckpointId.empty() || archive::isDigestHex(lastCheckpointId)) &&
         (lastPlanId.empty() || archive::isDigestHex(lastPlanId)) &&
         (lastPrunedHeight == 0 || lastCheckpointHeight > 0) && updatedAt > 0;
}

std::vector<std::pair<std::string, std::string>>
PruningManifestV2::fields() const {
  return {
      {"mode", nodeStorageModeToString(mode)},
      {"chainId", chainId},
      {"genesisConfigId", genesisConfigId},
      {"lastFinalizedHeight", std::to_string(lastFinalizedHeight)},
      {"lastCheckpointHeight", std::to_string(lastCheckpointHeight)},
      {"lastCheckpointId", orNone(lastCheckpointId)},
      {"lastPrunedHeight", std::to_string(lastPrunedHeight)},
      {"retentionBlocks", std::to_string(retentionBlocks)},
      {"retainedCheckpointSnapshots",
       std::to_string(retainedCheckpointSnapshots)},
      {"prunedBlockBodies", std::to_string(prunedBlockBodies)},
      {"prunedCheckpointSnapshots", std::to_string(prunedCheckpointSnapshots)},
      {"prunedLegacySnapshots", std::to_string(prunedLegacySnapshots)},
      {"lastPlanId", orNone(lastPlanId)},
      {"updatedAt", std::to_string(updatedAt)},
  };
}

PruningManifestV2
PruningManifestV2::fromFields(const std::map<std::string, std::string> &fields,
                              const std::string &prefix) {
  const auto field = [&](const std::string &key) {
    return requireField(fields, prefix + key);
  };
  PruningManifestV2 manifest;
  const auto mode = nodeStorageModeFromString(field("mode"));
  if (!mode || field("mode") != nodeStorageModeToString(*mode)) {
    throw std::invalid_argument("Unknown pruning manifest mode.");
  }
  manifest.mode = *mode;
  manifest.chainId = field("chainId");
  manifest.genesisConfigId = field("genesisConfigId");
  manifest.lastFinalizedHeight = parseU64(fields, prefix + "lastFinalizedHeight");
  manifest.lastCheckpointHeight =
      parseU64(fields, prefix + "lastCheckpointHeight");
  manifest.lastCheckpointId = fromNone(field("lastCheckpointId"));
  manifest.lastPrunedHeight = parseU64(fields, prefix + "lastPrunedHeight");
  manifest.retentionBlocks = parseU64(fields, prefix + "retentionBlocks");
  const std::uint64_t snapshots =
      parseU64(fields, prefix + "retainedCheckpointSnapshots");
  if (snapshots > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("Retained snapshot count is out of range.");
  }
  manifest.retainedCheckpointSnapshots = static_cast<std::uint32_t>(snapshots);
  manifest.prunedBlockBodies = parseU64(fields, prefix + "prunedBlockBodies");
  manifest.prunedCheckpointSnapshots =
      parseU64(fields, prefix + "prunedCheckpointSnapshots");
  manifest.prunedLegacySnapshots =
      parseU64(fields, prefix + "prunedLegacySnapshots");
  manifest.lastPlanId = fromNone(field("lastPlanId"));
  manifest.updatedAt = parseI64(fields, prefix + "updatedAt");
  if (!manifest.isValid()) {
    throw std::invalid_argument("Pruning manifest is inconsistent.");
  }
  return manifest;
}

std::string PruningManifestV2::toFileContents() const {
  if (!isValid()) {
    throw std::logic_error("Refusing to write an invalid pruning manifest.");
  }
  return serialization::KeyValueFileCodec::serialize(SCHEMA, fields());
}

PruningManifestV2
PruningManifestV2::fromFileContents(const std::string &contents) {
  const serialization::KeyValueFileDocument document =
      serialization::KeyValueFileCodec::parse(contents, SCHEMA);
  std::set<std::string> allowed;
  PruningManifestV2 manifest = fromFields(document.fields(), "");
  for (const auto &[key, value] : manifest.fields()) {
    (void)value;
    allowed.insert(key);
  }
  document.requireOnlyFields(allowed);
  if (manifest.toFileContents() != contents) {
    throw std::invalid_argument("Pruning manifest is not canonical.");
  }
  return manifest;
}

std::string PruningManifestV2::serializeJson() const {
  using utils::jsonString;
  std::ostringstream oss;
  oss << "{\"mode\":" << jsonString(nodeStorageModeToString(mode))
      << ",\"chainId\":" << jsonString(chainId)
      << ",\"lastFinalizedHeight\":" << lastFinalizedHeight
      << ",\"lastCheckpointHeight\":" << lastCheckpointHeight
      << ",\"lastCheckpointId\":" << jsonString(lastCheckpointId)
      << ",\"lastPrunedHeight\":" << lastPrunedHeight
      << ",\"retentionBlocks\":" << retentionBlocks
      << ",\"retainedCheckpointSnapshots\":" << retainedCheckpointSnapshots
      << ",\"prunedBlockBodies\":" << prunedBlockBodies
      << ",\"prunedCheckpointSnapshots\":" << prunedCheckpointSnapshots
      << ",\"prunedLegacySnapshots\":" << prunedLegacySnapshots
      << ",\"lastPlanId\":" << jsonString(lastPlanId)
      << ",\"updatedAt\":" << updatedAt << "}";
  return oss.str();
}

std::string pruningRunStatusToString(PruningRunStatus status) {
  switch (status) {
  case PruningRunStatus::APPLIED:
    return "APPLIED";
  case PruningRunStatus::NOOP:
    return "NOOP";
  case PruningRunStatus::DRY_RUN:
    return "DRY_RUN";
  case PruningRunStatus::REJECTED:
    return "REJECTED";
  }
  return "REJECTED";
}

bool HistoryPruningEngine::isAllowedTarget(const std::filesystem::path &relative) {
  if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
      relative.has_root_directory()) {
    return false;
  }
  std::vector<std::string> parts;
  for (const auto &part : relative) {
    const std::string text = part.string();
    if (text.empty() || text == "." || text == "..") {
      return false;
    }
    parts.push_back(text);
  }
  if (parts.size() == 3 && parts[0] == "history" && parts[1] == "snapshots") {
    return isNumbered(parts[2], "", ".snapshot");
  }
  if (parts.size() == 3 && parts[0] == "runtime" &&
      parts[1] == "fast_sync_snapshots") {
    return isNumbered(parts[2], "", ".fastsnap");
  }
  if (parts.size() == 2 && parts[0] == "blocks") {
    // block_<height>_<64 hex>.nodo
    const std::string &name = parts[1];
    const std::size_t separator = name.find('_', 6);
    if (name.rfind("block_", 0) != 0 || separator == std::string::npos ||
        name.size() != separator + 1 + 64 + 5 ||
        name.compare(name.size() - 5, 5, ".nodo") != 0) {
      return false;
    }
    return isNumbered(name.substr(0, separator), "block_", "") &&
           archive::isDigestHex(name.substr(separator + 1, 64));
  }
  return false;
}

std::optional<PruningManifestV2>
HistoryPruningEngine::loadManifest(const NodeDataDirectoryConfig &directory) {
  const auto path = directory.historyPruningManifestPath();
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return std::nullopt;
  }
  return PruningManifestV2::fromFileContents(
      storage::AtomicFile::readTextFile(path));
}

bool HistoryPruningEngine::shouldRunAutomatically(
    const config::HistoryParameters &parameters, std::uint64_t tipHeight) {
  if (!parameters.isValid() ||
      tipHeight < parameters.checkpointConfirmationBlocks()) {
    return false;
  }
  return parameters.isCheckpointHeight(tipHeight -
                                       parameters.checkpointConfirmationBlocks());
}

PruningRecoveryResult
HistoryPruningEngine::recover(const NodeDataDirectoryConfig &directory) {
  PruningRecoveryResult result;
  const std::filesystem::path journalPath = directory.pruningJournalPath();
  std::error_code ec;
  if (!std::filesystem::exists(journalPath, ec)) {
    return result;
  }
  Journal journal;
  try {
    journal = decodeJournal(storage::AtomicFile::readTextFile(journalPath));
  } catch (const std::exception &error) {
    // Never act on a journal we cannot fully trust: keep it for inspection.
    const auto quarantine =
        journalPath.parent_path() /
        ("journal.quarantined." +
         std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()));
    std::filesystem::rename(journalPath, quarantine, ec);
    result.status = PruningRecoveryStatus::QUARANTINED;
    result.reason = std::string("pruning journal rejected: ") + error.what();
    utils::log(utils::LogLevel::ERROR, "HistoryPruningEngine")
        << result.reason << std::endl;
    return result;
  }
  for (const auto &target : journal.targets) {
    removeTarget(directory, target);
    ++result.completedDeletes;
  }
  commitManifest(directory, journal.manifest);
  std::filesystem::remove(journalPath, ec);
  if (ec) {
    throw std::runtime_error("Unable to remove completed pruning journal: " +
                             ec.message());
  }
  result.status = PruningRecoveryStatus::RECOVERED;
  result.reason = "rolled interrupted pruning run " + journal.planId + " forward";
  return result;
}

PruningRunResult
HistoryPruningEngine::plan(const NodeDataDirectoryConfig &directory,
                           const config::GenesisConfig &genesisConfig,
                           const PruningOptions &options) {
  try {
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    return planRun(directory, genesisConfig, options, now).result;
  } catch (const std::exception &error) {
    return rejectedRun(error.what());
  }
}

PruningRunResult HistoryPruningEngine::run(const NodeDataDirectoryConfig &directory,
                                           const config::GenesisConfig &genesisConfig,
                                           const PruningOptions &options,
                                           std::int64_t now) {
  if (now <= 0) {
    return rejectedRun("pruning timestamp must be positive");
  }
  bool recovered = false;
  try {
    const PruningRecoveryResult recovery = recover(directory);
    if (recovery.status == PruningRecoveryStatus::QUARANTINED) {
      return rejectedRun(recovery.reason);
    }
    recovered = recovery.status == PruningRecoveryStatus::RECOVERED;

    PlannedRun planned = planRun(directory, genesisConfig, options, now);
    planned.result.recoveredInterruptedRun = recovered;
    if (planned.result.status == PruningRunStatus::REJECTED || options.dryRun) {
      return planned.result;
    }
    if (planned.targets.empty()) {
      commitManifest(directory, planned.manifest);
      planned.result.status = PruningRunStatus::NOOP;
      planned.result.reason = "pruning policy recorded; nothing is eligible";
      return planned.result;
    }

    // PREPARE
    const Journal journal{planned.manifest.lastPlanId, planned.targets,
                          planned.manifest};
    std::filesystem::create_directories(directory.historyPruningDirectoryPath());
    storage::AtomicFile::writeTextFile(directory.pruningJournalPath(),
                                       encodeJournal(journal));
    if (options.crashPoint == PruningCrashPoint::AFTER_PREPARE) {
      throw std::runtime_error("simulated crash after PREPARE");
    }
    // DELETE
    bool first = true;
    for (const auto &target : planned.targets) {
      removeTarget(directory, target);
      if (first && options.crashPoint == PruningCrashPoint::AFTER_FIRST_DELETE) {
        throw std::runtime_error("simulated crash after the first DELETE");
      }
      first = false;
    }
    if (options.crashPoint == PruningCrashPoint::BEFORE_COMMIT) {
      throw std::runtime_error("simulated crash before COMMIT");
    }
    // COMMIT
    commitManifest(directory, planned.manifest);
    std::error_code ec;
    std::filesystem::remove(directory.pruningJournalPath(), ec);
    planned.result.status = PruningRunStatus::APPLIED;
    planned.result.reason = "removed " +
                            std::to_string(planned.targets.size()) +
                            " file(s) released by every retention rule";
    return planned.result;
  } catch (const std::exception &error) {
    PruningRunResult result = rejectedRun(error.what());
    result.recoveredInterruptedRun = recovered;
    return result;
  }
}

std::string HistoryPruningEngine::migrateLegacyManifest(
    const NodeDataDirectoryConfig &directory) {
  const auto legacyPath = directory.pruningManifestPath();
  std::error_code ec;
  if (!std::filesystem::exists(legacyPath, ec)) {
    return "";
  }
  const NodePruningManifest legacy = NodePruningManifest::fromFileContents(
      storage::AtomicFile::readTextFile(legacyPath));
  PruningManifestV2 manifest;
  switch (legacy.config().mode()) {
  case NodePruningMode::ARCHIVE:
    manifest.mode = NodeStorageMode::ARCHIVE;
    break;
  case NodePruningMode::FULL:
    manifest.mode = NodeStorageMode::NORMAL;
    break;
  case NodePruningMode::LIGHT:
    manifest.mode = NodeStorageMode::LIGHT;
    break;
  }
  manifest.chainId = legacy.chainId();
  manifest.genesisConfigId = legacy.genesisConfigId();
  manifest.lastFinalizedHeight = legacy.latestHeight();
  manifest.prunedBlockBodies = legacy.prunedBlockArtifactCount();
  manifest.prunedLegacySnapshots = legacy.prunedSnapshotCount();
  // Legacy pruning removed block files without a verified checkpoint; record
  // the boundary but no checkpoint base so the loss stays visible.
  manifest.lastPrunedHeight =
      legacy.prunedBlockArtifactCount() > 0 ? legacy.retainFromHeight() : 0;
  if (manifest.lastPrunedHeight > 0) {
    manifest.lastCheckpointHeight = std::min<std::uint64_t>(
        legacy.snapshotBoundaryHeight(), manifest.lastFinalizedHeight);
    if (manifest.lastCheckpointHeight == 0) {
      manifest.lastCheckpointHeight = manifest.lastFinalizedHeight;
    }
  }
  manifest.updatedAt = std::max<std::int64_t>(legacy.updatedAt(), 1);
  commitManifest(directory, manifest);
  std::filesystem::remove(legacyPath, ec);
  if (ec) {
    throw std::runtime_error("Unable to remove the legacy pruning manifest: " +
                             ec.message());
  }
  return legacy.prunedBlockArtifactCount() > 0
             ? "converted legacy pruning manifest; WARNING: legacy LIGHT "
               "pruning removed " +
                   std::to_string(legacy.prunedBlockArtifactCount()) +
                   " finalized block files, which this node needs to reload; "
                   "restore them from an archive node"
             : "converted legacy pruning manifest to the v2 manifest";
}

} // namespace nodo::node
