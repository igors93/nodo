#include "node/history/StorageStatus.hpp"

#include "config/HistoryParameters.hpp"
#include "node/history/HistoryPruningEngine.hpp"
#include "node/history/HistoryStore.hpp"
#include "node/history/StorageMigration.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/JsonText.hpp"

#include <set>
#include <sstream>
#include <stdexcept>

namespace nodo::node {

namespace {

StorageCategoryUsage usageOf(const std::string &category,
                             const std::filesystem::path &directory,
                             bool recursive) {
  StorageCategoryUsage usage;
  usage.category = category;
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec)) {
    return usage;
  }
  const auto account = [&usage](const std::filesystem::directory_entry &entry) {
    std::error_code sizeError;
    if (entry.is_regular_file(sizeError) && !entry.is_symlink(sizeError)) {
      ++usage.files;
      usage.bytes += entry.file_size(sizeError);
    }
  };
  if (recursive) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(directory, ec)) {
      account(entry);
    }
  } else {
    for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
      account(entry);
    }
  }
  return usage;
}

std::uint64_t parseCounter(const std::map<std::string, std::string> &fields,
                           const std::string &key) {
  const auto found = fields.find(key);
  if (found == fields.end() || found->second.empty() ||
      found->second.size() > 20 ||
      found->second.find_first_not_of("0123456789") != std::string::npos) {
    throw std::invalid_argument("Malformed self-audit field: " + key);
  }
  return std::stoull(found->second);
}

} // namespace

std::string ArchivalSelfAuditCounters::toFileContents() const {
  return serialization::KeyValueFileCodec::serialize(
      SCHEMA, {{"challenges", std::to_string(challenges)},
               {"passed", std::to_string(passed)},
               {"failed", std::to_string(failed)},
               {"lastSegment", std::to_string(lastSegment)},
               {"updatedAt", std::to_string(updatedAt)}});
}

ArchivalSelfAuditCounters
ArchivalSelfAuditCounters::fromFileContents(const std::string &contents) {
  const serialization::KeyValueFileDocument document =
      serialization::KeyValueFileCodec::parse(contents, SCHEMA);
  document.requireOnlyFields(
      {"challenges", "passed", "failed", "lastSegment", "updatedAt"});
  ArchivalSelfAuditCounters counters;
  counters.challenges = parseCounter(document.fields(), "challenges");
  counters.passed = parseCounter(document.fields(), "passed");
  counters.failed = parseCounter(document.fields(), "failed");
  counters.lastSegment = parseCounter(document.fields(), "lastSegment");
  counters.updatedAt =
      static_cast<std::int64_t>(parseCounter(document.fields(), "updatedAt"));
  if (counters.passed + counters.failed > counters.challenges) {
    throw std::invalid_argument("Self-audit counters are inconsistent.");
  }
  return counters;
}

std::optional<ArchivalSelfAuditCounters>
ArchivalSelfAuditCounters::load(const NodeDataDirectoryConfig &directory) {
  const auto path = directory.archiveDirectoryPath() / "self_audit.nodo";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return std::nullopt;
  }
  return fromFileContents(storage::AtomicFile::readTextFile(path));
}

void ArchivalSelfAuditCounters::save(
    const NodeDataDirectoryConfig &directory) const {
  std::filesystem::create_directories(directory.archiveDirectoryPath());
  storage::AtomicFile::writeTextFile(
      directory.archiveDirectoryPath() / "self_audit.nodo", toFileContents());
}

StorageStatusReport
StorageStatusReport::collect(const NodeDataDirectoryConfig &directory) {
  StorageStatusReport report;
  report.schemaVersion = StorageMigration::schemaVersion(directory);
  report.historyLayout = StorageMigration::supportsHistoryLayout(directory);
  const NodeDataDirectoryReadResult manifest =
      NodeDataDirectory::loadManifest(directory);
  if (manifest.loaded()) {
    report.networkName = manifest.manifest().networkName();
    report.finalizedHeight = manifest.manifest().latestBlockHeight();
  }

  const HistoryStore store(directory);
  const std::vector<std::uint64_t> checkpoints = store.checkpointHeights();
  report.checkpointCount = checkpoints.size();
  report.checkpointConflicts = store.conflictCount();
  report.checkpointSnapshots = store.snapshotHeights().size();
  if (!checkpoints.empty()) {
    try {
      if (const auto latest = store.loadCheckpoint(checkpoints.back())) {
        report.latestCheckpointHeight = latest->height();
        report.latestCheckpointId = latest->checkpointId();
        report.latestCheckpointTimestamp = latest->fields().blockTimestamp;
      }
    } catch (const std::exception &) {
      report.latestCheckpointId = "CORRUPT";
    }
    report.checkpointAgeBlocks =
        report.finalizedHeight >= report.latestCheckpointHeight
            ? report.finalizedHeight - report.latestCheckpointHeight
            : 0;
  }
  try {
    const config::HistoryParameters parameters =
        config::HistoryParameters::forNetwork(report.networkName);
    report.nextCheckpointHeight =
        parameters.checkpointHeightAtOrBelow(report.finalizedHeight) +
        parameters.checkpointIntervalBlocks();
    report.archiveSegmentsSealed =
        parameters.sealedSegmentCount(report.finalizedHeight);
  } catch (const std::exception &) {
  }

  report.archiveSegments = store.contiguousSegmentCommitmentCount();
  if (const auto commitments = store.loadSegmentCommitments(report.archiveSegments)) {
    for (const auto &commitment : *commitments) {
      report.archiveBytes += commitment.totalBytes();
    }
  }
  try {
    if (const auto manifestV2 = HistoryPruningEngine::loadManifest(directory)) {
      report.mode = nodeStorageModeToString(manifestV2->mode);
      report.prunedHeight = manifestV2->lastPrunedHeight;
    }
  } catch (const std::exception &) {
    report.mode = "CORRUPT_MANIFEST";
  }
  std::error_code ec;
  report.pruningJournalPending =
      std::filesystem::exists(directory.pruningJournalPath(), ec);
  try {
    if (const auto audit = ArchivalSelfAuditCounters::load(directory)) {
      report.selfAudit = *audit;
    }
  } catch (const std::exception &) {
  }

  report.usage = {
      usageOf("blocks", directory.blocksDirectoryPath(), false),
      usageOf("checkpoints", directory.checkpointsDirectoryPath(), true),
      usageOf("checkpoint_snapshots",
              directory.checkpointSnapshotsDirectoryPath(), false),
      usageOf("archive", directory.archiveDirectoryPath(), true),
      usageOf("legacy_fast_sync_snapshots",
              directory.fastSyncSnapshotsDirectoryPath(), false),
      usageOf("mempool", directory.mempoolDirectoryPath(), false),
      usageOf("runtime", directory.runtimeDirectoryPath(), false),
  };
  for (const StorageCategoryUsage &usage : report.usage) {
    report.totalBytes += usage.bytes;
  }
  return report;
}

std::string StorageStatusReport::serializeJson() const {
  using utils::jsonString;
  std::ostringstream oss;
  oss << "{\"networkName\":" << jsonString(networkName)
      << ",\"schemaVersion\":" << schemaVersion
      << ",\"historyLayout\":" << (historyLayout ? "true" : "false")
      << ",\"mode\":" << jsonString(mode)
      << ",\"finalizedHeight\":" << finalizedHeight
      << ",\"checkpoints\":{\"count\":" << checkpointCount
      << ",\"latestHeight\":" << latestCheckpointHeight
      << ",\"latestId\":" << jsonString(latestCheckpointId)
      << ",\"latestTimestamp\":" << latestCheckpointTimestamp
      << ",\"ageBlocks\":" << checkpointAgeBlocks
      << ",\"nextHeight\":" << nextCheckpointHeight
      << ",\"snapshots\":" << checkpointSnapshots
      << ",\"conflicts\":" << checkpointConflicts << "}"
      << ",\"pruning\":{\"prunedHeight\":" << prunedHeight
      << ",\"journalPending\":" << (pruningJournalPending ? "true" : "false")
      << "}"
      << ",\"archive\":{\"segments\":" << archiveSegments
      << ",\"sealedSegments\":" << archiveSegmentsSealed
      << ",\"bytes\":" << archiveBytes
      << ",\"selfAudit\":{\"challenges\":" << selfAudit.challenges
      << ",\"passed\":" << selfAudit.passed
      << ",\"failed\":" << selfAudit.failed << "}}"
      << ",\"usage\":[";
  for (std::size_t index = 0; index < usage.size(); ++index) {
    oss << (index == 0 ? "" : ",") << "{\"category\":"
        << jsonString(usage[index].category)
        << ",\"files\":" << usage[index].files
        << ",\"bytes\":" << usage[index].bytes << "}";
  }
  oss << "],\"totalBytes\":" << totalBytes << "}";
  return oss.str();
}

std::string StorageStatusReport::serializeText() const {
  std::ostringstream oss;
  oss << "Storage schema version: " << schemaVersion
      << (historyLayout ? " (history layout)" : " (run `nodo storage migrate`)")
      << "\n"
      << "Storage mode: " << mode << "\n"
      << "Finalized height: " << finalizedHeight << "\n"
      << "Checkpoints: " << checkpointCount << " (latest " << latestCheckpointHeight
      << ", age " << checkpointAgeBlocks << " blocks, next "
      << nextCheckpointHeight << ")\n"
      << "Latest checkpoint id: "
      << (latestCheckpointId.empty() ? "NONE" : latestCheckpointId) << "\n"
      << "Checkpoint snapshots: " << checkpointSnapshots << "\n"
      << "Checkpoint conflicts: " << checkpointConflicts << "\n"
      << "Pruned block bodies below height: " << prunedHeight << "\n"
      << "Pruning journal pending: " << (pruningJournalPending ? "yes" : "no")
      << "\n"
      << "Archive segment commitments: " << archiveSegments << " of "
      << archiveSegmentsSealed << " sealed (" << archiveBytes << " bytes)\n"
      << "Archival self-audit: " << selfAudit.passed << "/"
      << selfAudit.challenges << " passed\n";
  for (const StorageCategoryUsage &entry : usage) {
    oss << "  " << entry.category << ": " << entry.files << " files, "
        << entry.bytes << " bytes\n";
  }
  oss << "Total: " << totalBytes << " bytes\n";
  return oss.str();
}

} // namespace nodo::node
