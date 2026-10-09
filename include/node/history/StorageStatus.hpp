#ifndef NODO_NODE_HISTORY_STORAGE_STATUS_HPP
#define NODO_NODE_HISTORY_STORAGE_STATUS_HPP

#include "node/NodeDataDirectory.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

struct StorageCategoryUsage {
  std::string category;
  std::uint64_t files = 0;
  std::uint64_t bytes = 0;
};

// Local Proof-of-Archival self-audit counters (history/archive/self_audit.nodo).
struct ArchivalSelfAuditCounters {
  static constexpr const char *SCHEMA = "NODO_ARCHIVAL_SELF_AUDIT_V1";

  std::uint64_t challenges = 0;
  std::uint64_t passed = 0;
  std::uint64_t failed = 0;
  std::uint64_t lastSegment = 0;
  std::int64_t updatedAt = 0;

  std::string toFileContents() const;
  static ArchivalSelfAuditCounters fromFileContents(const std::string &contents);
  static std::optional<ArchivalSelfAuditCounters>
  load(const NodeDataDirectoryConfig &directory);
  void save(const NodeDataDirectoryConfig &directory) const;
};

/*
 * Read-only storage report for CLI, JSON-RPC and metrics (ADR 0014). It
 * scans the data directory, so callers should not invoke it per block.
 */
struct StorageStatusReport {
  std::string networkName;
  std::uint64_t schemaVersion = 0;
  bool historyLayout = false;
  std::string mode = "ARCHIVE";
  std::uint64_t finalizedHeight = 0;

  std::uint64_t checkpointCount = 0;
  std::uint64_t latestCheckpointHeight = 0;
  std::string latestCheckpointId;
  std::int64_t latestCheckpointTimestamp = 0;
  std::uint64_t checkpointAgeBlocks = 0;
  std::uint64_t nextCheckpointHeight = 0;
  std::uint64_t checkpointConflicts = 0;
  std::uint64_t checkpointSnapshots = 0;

  std::uint64_t prunedHeight = 0;
  bool pruningJournalPending = false;

  std::uint64_t archiveSegments = 0;
  std::uint64_t archiveSegmentsSealed = 0;
  std::uint64_t archiveBytes = 0;
  ArchivalSelfAuditCounters selfAudit;

  std::vector<StorageCategoryUsage> usage;
  std::uint64_t totalBytes = 0;

  static StorageStatusReport collect(const NodeDataDirectoryConfig &directory);
  std::string serializeJson() const;
  std::string serializeText() const;
};

} // namespace nodo::node

#endif
