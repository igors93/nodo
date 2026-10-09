#ifndef NODO_NODE_HISTORY_STORAGE_MIGRATION_HPP
#define NODO_NODE_HISTORY_STORAGE_MIGRATION_HPP

#include "node/NodeDataDirectory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

struct StorageMigrationResult {
  bool success = false;
  std::uint64_t fromVersion = 0;
  std::uint64_t toVersion = 0;
  std::vector<std::string> actions;
  std::string reason;
};

/*
 * Explicit, idempotent node data directory migrations (roadmap 3.15).
 *
 * v1 -> v2 creates the history/ tree and converts a legacy pruning manifest
 * into the v2 pruning manifest. The schema file is rewritten last, so a crash
 * at any point leaves a directory that still reads as v1 and the migration
 * can simply run again. Older binaries refuse a v2 directory instead of
 * misreading its history files. Nodo never migrates implicitly.
 */
class StorageMigration {
public:
  static constexpr std::uint64_t kHistoryLayoutVersion = 2;

  // 0 when the schema file is missing or invalid.
  static std::uint64_t schemaVersion(const NodeDataDirectoryConfig &directory);
  static bool supportsHistoryLayout(const NodeDataDirectoryConfig &directory);
  static StorageMigrationResult migrate(const NodeDataDirectoryConfig &directory);
};

} // namespace nodo::node

#endif
