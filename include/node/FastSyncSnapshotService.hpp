#ifndef NODO_NODE_FAST_SYNC_SNAPSHOT_SERVICE_HPP
#define NODO_NODE_FAST_SYNC_SNAPSHOT_SERVICE_HPP

#include "node/FastSyncSnapshot.hpp"
#include "node/FastSyncSnapshotStore.hpp"
#include "node/FastSyncSnapshotVerifier.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/PersistentBlockStateSync.hpp"

#include <optional>
#include <string>

namespace nodo::node {

enum class FastSyncSnapshotImportStatus { IMPORTED, REJECTED };

class FastSyncSnapshotImportResult {
public:
  static FastSyncSnapshotImportResult
  imported(PersistentSyncCheckpoint checkpoint);
  static FastSyncSnapshotImportResult rejected(std::string reason);

  FastSyncSnapshotImportStatus status() const;
  const std::string &reason() const;
  bool imported() const;
  const std::optional<PersistentSyncCheckpoint> &checkpoint() const;

private:
  FastSyncSnapshotImportResult();
  FastSyncSnapshotImportStatus m_status;
  std::string m_reason;
  std::optional<PersistentSyncCheckpoint> m_checkpoint;
};

class FastSyncSnapshotService {
public:
  static FastSyncSnapshot buildSnapshot(const NodeRuntime &runtime,
                                        std::int64_t now);

  static bool persistSnapshot(const NodeDataDirectoryConfig &directoryConfig,
                              const FastSyncSnapshot &snapshot);

  /*
   * Validate a locally available snapshot payload against a remote manifest
   * and write a sync checkpoint.  This function deliberately performs no
   * partial in-memory mutation: callers must only publish a hydrated runtime
   * after the snapshot and post-snapshot blocks are both validated.
   */
  static FastSyncSnapshotImportResult
  verifyAndCheckpoint(const FastSyncSnapshot &snapshot,
                      const config::GenesisConfig &genesisConfig,
                      const PersistentSnapshotSyncManifest &manifest,
                      PersistentSyncCheckpointStore *checkpointStore,
                      std::int64_t now);
};

} // namespace nodo::node

#endif
