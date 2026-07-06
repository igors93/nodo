#include "node/FastSyncSnapshotService.hpp"

#include <filesystem>
#include <utility>

namespace nodo::node {

FastSyncSnapshotImportResult::FastSyncSnapshotImportResult()
    : m_status(FastSyncSnapshotImportStatus::REJECTED),
      m_reason("Uninitialized fast-sync snapshot import result."),
      m_checkpoint(std::nullopt) {}

FastSyncSnapshotImportResult FastSyncSnapshotImportResult::imported(
    PersistentSyncCheckpoint checkpoint
) {
    FastSyncSnapshotImportResult result;
    result.m_status = FastSyncSnapshotImportStatus::IMPORTED;
    result.m_reason = "";
    result.m_checkpoint = std::move(checkpoint);
    return result;
}

FastSyncSnapshotImportResult FastSyncSnapshotImportResult::rejected(
    std::string reason
) {
    FastSyncSnapshotImportResult result;
    result.m_status = FastSyncSnapshotImportStatus::REJECTED;
    result.m_reason = std::move(reason);
    return result;
}

FastSyncSnapshotImportStatus FastSyncSnapshotImportResult::status() const {
    return m_status;
}

const std::string& FastSyncSnapshotImportResult::reason() const {
    return m_reason;
}

bool FastSyncSnapshotImportResult::imported() const {
    return m_status == FastSyncSnapshotImportStatus::IMPORTED &&
           m_checkpoint.has_value();
}

const std::optional<PersistentSyncCheckpoint>&
FastSyncSnapshotImportResult::checkpoint() const {
    return m_checkpoint;
}

FastSyncSnapshot FastSyncSnapshotService::buildSnapshot(
    const NodeRuntime& runtime,
    std::int64_t now
) {
    return FastSyncSnapshot::fromRuntime(runtime, now);
}

bool FastSyncSnapshotService::persistSnapshot(
    const NodeDataDirectoryConfig& directoryConfig,
    const FastSyncSnapshot& snapshot
) {
    if (!directoryConfig.isValid() || !snapshot.isValid()) {
        return false;
    }
    const FastSyncSnapshotStore store(
        directoryConfig.fastSyncSnapshotsDirectoryPath()
    );
    return store.save(snapshot);
}

FastSyncSnapshotImportResult FastSyncSnapshotService::verifyAndCheckpoint(
    const FastSyncSnapshot& snapshot,
    const config::GenesisConfig& genesisConfig,
    const PersistentSnapshotSyncManifest& manifest,
    PersistentSyncCheckpointStore* checkpointStore,
    std::int64_t now
) {
    const FastSyncSnapshotVerificationResult verification =
        FastSyncSnapshotVerifier::verifyAgainstManifest(
            snapshot,
            genesisConfig,
            manifest
        );

    if (!verification.accepted()) {
        return FastSyncSnapshotImportResult::rejected(
            "fast-sync snapshot verification failed: " + verification.reason()
        );
    }

    PersistentSyncCheckpoint checkpoint(
        PersistentSyncCheckpoint::SCHEMA_VERSION,
        snapshot.blockHeight(),
        snapshot.blockHash(),
        snapshot.stateRoot(),
        PersistentSyncStatus::COMPLETE,
        manifest.sourcePeerId(),
        now
    );

    if (!checkpoint.isValid()) {
        return FastSyncSnapshotImportResult::rejected(
            "verified fast-sync snapshot produced an invalid checkpoint"
        );
    }

    if (checkpointStore != nullptr) {
        const PersistentSyncCheckpointWriteResult write =
            checkpointStore->save(checkpoint);
        if (!write.isSaved()) {
            return FastSyncSnapshotImportResult::rejected(
                "failed to persist fast-sync checkpoint: " + write.reason()
            );
        }
    }

    return FastSyncSnapshotImportResult::imported(std::move(checkpoint));
}

} // namespace nodo::node
