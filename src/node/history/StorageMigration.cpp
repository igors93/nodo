#include "node/history/StorageMigration.hpp"

#include "node/history/HistoryPruningEngine.hpp"
#include "storage/StorageSchemaVersion.hpp"

#include <exception>

namespace nodo::node {

std::uint64_t
StorageMigration::schemaVersion(const NodeDataDirectoryConfig &directory) {
  if (!directory.isValid()) {
    return 0;
  }
  const storage::StorageSchemaValidationResult validation =
      storage::StorageSchemaVersionFile::validateNodeDataDirectoryRoot(
          directory.rootPath());
  return validation.accepted() ? validation.schema()->version() : 0;
}

bool StorageMigration::supportsHistoryLayout(
    const NodeDataDirectoryConfig &directory) {
  return schemaVersion(directory) >= kHistoryLayoutVersion;
}

StorageMigrationResult
StorageMigration::migrate(const NodeDataDirectoryConfig &directory) {
  StorageMigrationResult result;
  const std::uint64_t current =
      storage::StorageSchemaVersion::currentNodeDataDirectoryVersion();
  result.toVersion = current;
  result.fromVersion = schemaVersion(directory);
  if (result.fromVersion == 0) {
    result.reason = "data directory has no valid storage schema; refusing to "
                    "migrate an unknown layout";
    return result;
  }
  if (result.fromVersion == current) {
    result.success = true;
    result.reason = "storage schema is already current";
    return result;
  }
  try {
    if (result.fromVersion < kHistoryLayoutVersion) {
      NodeDataDirectory::ensureHistoryDirectoryTree(directory);
      result.actions.push_back("created history/ directory tree");
      const std::string converted =
          HistoryPruningEngine::migrateLegacyManifest(directory);
      if (!converted.empty()) {
        result.actions.push_back(converted);
      }
    }
    // The schema file is the commit point of the migration.
    storage::StorageSchemaVersion::writeCurrentNodeDataDirectoryVersionFile(
        directory.rootPath());
    result.actions.push_back("wrote storage schema version " +
                             std::to_string(current));
    result.success = schemaVersion(directory) == current;
    if (!result.success) {
      result.reason = "storage schema did not read back as current";
    }
  } catch (const std::exception &error) {
    result.success = false;
    result.reason = error.what();
  }
  return result;
}

} // namespace nodo::node
