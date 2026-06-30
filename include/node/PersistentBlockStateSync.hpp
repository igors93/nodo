#ifndef NODO_NODE_PERSISTENT_BLOCK_STATE_SYNC_HPP
#define NODO_NODE_PERSISTENT_BLOCK_STATE_SYNC_HPP

#include "core/Blockchain.hpp"
#include "core/ProtocolLimits.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/ChainSyncMessages.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class NodeRuntime;
class NodeDataDirectoryConfig;

constexpr std::uint64_t NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH =
    core::ProtocolLimits::MAX_PERSISTENT_SYNC_BLOCK_BATCH;

/*
 * PersistentBlock/State sync is the durable boundary used by the testnet node
 * runtime to remember how far local sync progressed and to validate ordered
 * block batches before they are accepted as sync progress.
 *
 * This phase intentionally does not mutate Blockchain directly. It prepares the
 * safe and testable sync plan/apply/persist primitives that the runtime can call
 * after existing block/state validators accept real block data.
 */
enum class PersistentSyncStatus {
    UNKNOWN,
    IDLE,
    SYNCING,
    COMPLETE,
    FAILED
};

std::string persistentSyncStatusToString(PersistentSyncStatus status);
PersistentSyncStatus persistentSyncStatusFromString(const std::string& value);

class PersistentSyncCheckpoint {
public:
    static constexpr const char* SCHEMA_VERSION =
        "NODO_PERSISTENT_SYNC_CHECKPOINT_V1";

    PersistentSyncCheckpoint();

    PersistentSyncCheckpoint(
        std::string schemaVersion,
        std::uint64_t finalizedHeight,
        std::string finalizedBlockHash,
        std::string finalizedStateRoot,
        PersistentSyncStatus status,
        std::string sourcePeerId,
        std::int64_t updatedAt
    );

    const std::string& schemaVersion() const;
    std::uint64_t finalizedHeight() const;
    const std::string& finalizedBlockHash() const;
    const std::string& finalizedStateRoot() const;
    PersistentSyncStatus status() const;
    const std::string& sourcePeerId() const;
    std::int64_t updatedAt() const;

    bool isValid() const;
    std::string serialize() const;

    static PersistentSyncCheckpoint genesis(
        const std::string& genesisBlockHash,
        const std::string& genesisStateRoot,
        std::int64_t now
    );

    static PersistentSyncCheckpoint deserialize(
        const std::string& serialized
    );

private:
    std::string m_schemaVersion;
    std::uint64_t m_finalizedHeight;
    std::string m_finalizedBlockHash;
    std::string m_finalizedStateRoot;
    PersistentSyncStatus m_status;
    std::string m_sourcePeerId;
    std::int64_t m_updatedAt;
};

enum class PersistentSyncCheckpointReadStatus {
    LOADED,
    MISSING,
    MALFORMED,
    INVALID,
    IO_FAILURE
};

std::string persistentSyncCheckpointReadStatusToString(
    PersistentSyncCheckpointReadStatus status
);

class PersistentSyncCheckpointReadResult {
public:
    static PersistentSyncCheckpointReadResult loaded(PersistentSyncCheckpoint checkpoint);
    static PersistentSyncCheckpointReadResult missing();
    static PersistentSyncCheckpointReadResult malformed(std::string reason);
    static PersistentSyncCheckpointReadResult invalid(std::string reason);
    static PersistentSyncCheckpointReadResult ioFailure(std::string reason);

    PersistentSyncCheckpointReadStatus status() const;
    const std::string& reason() const;
    bool loaded() const;
    const PersistentSyncCheckpoint& checkpoint() const;

private:
    PersistentSyncCheckpointReadResult();
    PersistentSyncCheckpointReadStatus m_status;
    std::string m_reason;
    PersistentSyncCheckpoint m_checkpoint;
};

enum class PersistentSyncCheckpointWriteStatus {
    SAVED,
    INVALID_CHECKPOINT,
    IO_FAILURE
};

std::string persistentSyncCheckpointWriteStatusToString(
    PersistentSyncCheckpointWriteStatus status
);

class PersistentSyncCheckpointWriteResult {
public:
    static PersistentSyncCheckpointWriteResult saved();
    static PersistentSyncCheckpointWriteResult invalidCheckpoint(std::string reason);
    static PersistentSyncCheckpointWriteResult ioFailure(std::string reason);

    PersistentSyncCheckpointWriteStatus status() const;
    const std::string& reason() const;
    bool isSaved() const;

private:
    PersistentSyncCheckpointWriteResult();
    PersistentSyncCheckpointWriteStatus m_status;
    std::string m_reason;
};

class PersistentSyncCheckpointStore {
public:
    explicit PersistentSyncCheckpointStore(
        std::filesystem::path dataDirectory
    );

    const std::filesystem::path& dataDirectory() const;
    std::filesystem::path checkpointFilePath() const;

    bool exists() const;

    PersistentSyncCheckpointReadResult read() const;

    PersistentSyncCheckpointWriteResult save(
        const PersistentSyncCheckpoint& checkpoint
    ) const;

private:
    std::filesystem::path m_dataDirectory;
};

class PersistentBlockSyncItem {
public:
    PersistentBlockSyncItem();

    PersistentBlockSyncItem(
        std::uint64_t height,
        std::string blockHash,
        std::string previousBlockHash,
        std::string serializedBlock,
        std::string finalizedStateRoot,
        std::int64_t createdAt,
        std::string serializedFinalizedRecord = ""
    );

    std::uint64_t height() const;
    const std::string& blockHash() const;
    const std::string& previousBlockHash() const;
    const std::string& serializedBlock() const;
    const std::string& finalizedStateRoot() const;
    std::int64_t createdAt() const;
    const std::string& serializedFinalizedRecord() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_height;
    std::string m_blockHash;
    std::string m_previousBlockHash;
    std::string m_serializedBlock;
    std::string m_finalizedStateRoot;
    std::int64_t m_createdAt;
    std::string m_serializedFinalizedRecord;
};

class PersistentBlockSyncBatch {
public:
    PersistentBlockSyncBatch();

    PersistentBlockSyncBatch(
        std::string sourcePeerId,
        std::uint64_t fromHeight,
        std::uint64_t toHeight,
        std::vector<PersistentBlockSyncItem> items,
        std::int64_t createdAt
    );

    const std::string& sourcePeerId() const;
    std::uint64_t fromHeight() const;
    std::uint64_t toHeight() const;
    const std::vector<PersistentBlockSyncItem>& items() const;
    std::int64_t createdAt() const;

    std::uint64_t blockCount() const;
    const PersistentBlockSyncItem* lastItem() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_sourcePeerId;
    std::uint64_t m_fromHeight;
    std::uint64_t m_toHeight;
    std::vector<PersistentBlockSyncItem> m_items;
    std::int64_t m_createdAt;
};

class PersistentSnapshotSyncManifest {
public:
    PersistentSnapshotSyncManifest();

    PersistentSnapshotSyncManifest(
        std::string sourcePeerId,
        std::uint64_t snapshotHeight,
        std::string snapshotBlockHash,
        std::string snapshotStateRoot,
        std::string snapshotDigest,
        std::int64_t createdAt
    );

    const std::string& sourcePeerId() const;
    std::uint64_t snapshotHeight() const;
    const std::string& snapshotBlockHash() const;
    const std::string& snapshotStateRoot() const;
    const std::string& snapshotDigest() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

    static PersistentSnapshotSyncManifest deserialize(
        const std::string& serialized
    );

private:
    std::string m_sourcePeerId;
    std::uint64_t m_snapshotHeight;
    std::string m_snapshotBlockHash;
    std::string m_snapshotStateRoot;
    std::string m_snapshotDigest;
    std::int64_t m_createdAt;
};

enum class PersistentSyncPlanStatus {
    NOT_REQUIRED,
    REQUEST_BLOCKS,
    REQUEST_SNAPSHOT,
    REJECTED
};

std::string persistentSyncPlanStatusToString(
    PersistentSyncPlanStatus status
);

class PersistentSyncPlan {
public:
    // Minimum height gap that triggers a snapshot request instead of block-by-block sync.
    static constexpr std::uint64_t SNAPSHOT_GAP_THRESHOLD = 500;

    PersistentSyncPlan();

    PersistentSyncPlan(
        PersistentSyncPlanStatus status,
        std::string reason,
        std::optional<NetworkBlockSyncRequest> blockRequest,
        std::optional<PersistentSnapshotSyncManifest> snapshotRequest = std::nullopt
    );

    PersistentSyncPlanStatus status() const;
    const std::string& reason() const;
    const std::optional<NetworkBlockSyncRequest>& blockRequest() const;
    const std::optional<PersistentSnapshotSyncManifest>& snapshotRequest() const;

    bool requestBlocks() const;
    bool requestSnapshot() const;
    bool notRequired() const;
    bool rejected() const;
    std::string serialize() const;

private:
    PersistentSyncPlanStatus m_status;
    std::string m_reason;
    std::optional<NetworkBlockSyncRequest> m_blockRequest;
    std::optional<PersistentSnapshotSyncManifest> m_snapshotRequest;
};

class PersistentBlockStateSyncPlanner {
public:
    static PersistentSyncPlan planFromRemoteStatus(
        const PersistentSyncCheckpoint& localCheckpoint,
        const ChainStatusMessage& remoteStatus,
        const std::string& localNodeId,
        const std::string& sourcePeerId,
        std::uint64_t maxBlocksPerRequest,
        std::int64_t now
    );

};

enum class PersistentSyncApplyStatus {
    APPLIED,
    REJECTED
};

std::string persistentSyncApplyStatusToString(
    PersistentSyncApplyStatus status
);

class PersistentSyncApplyResult {
public:
    PersistentSyncApplyResult();

    PersistentSyncApplyResult(
        PersistentSyncApplyStatus status,
        std::string reason,
        std::optional<PersistentSyncCheckpoint> checkpoint
    );

    PersistentSyncApplyStatus status() const;
    const std::string& reason() const;
    const std::optional<PersistentSyncCheckpoint>& checkpoint() const;
    bool applied() const;
    std::string serialize() const;

private:
    PersistentSyncApplyStatus m_status;
    std::string m_reason;
    std::optional<PersistentSyncCheckpoint> m_checkpoint;
};

class PersistentBlockStateSyncApplier {
public:
    /*
     * Import a validated finalized batch into the runtime.
     *
     * If `checkpointStore` is non-null, the updated checkpoint is persisted
     * to disk BEFORE the in-memory runtime is mutated. This ensures that a
     * crash between FinalizedBlockStore::persistBatch and the runtime update
     * does not leave the checkpoint behind the on-disk block state.
     */
    static PersistentSyncApplyResult importFinalizedBatch(
        const PersistentSyncCheckpoint& checkpoint,
        const PersistentBlockSyncBatch& batch,
        NodeRuntime& runtime,
        const NodeDataDirectoryConfig& directoryConfig,
        PersistentSyncCheckpointStore* checkpointStore,
        std::int64_t now
    );

    /*
     * Import a state snapshot manifest as a fast-sync starting point.
     *
     * Validates that the manifest's snapshotDigest matches the canonical hash
     * of the provided manifest, sets the checkpoint to the snapshot height and
     * state root, and records the new height in the runtime blockchain via a
     * synthetic genesis-equivalent origin. This allows the node to resume block
     * sync from a trusted snapshot rather than replaying from height 0.
     *
     * Returns REJECTED if the manifest is invalid, the digest does not match,
     * or the snapshot height is not ahead of the current checkpoint.
     *
     * Note: This is a skeleton implementation. Full account-state hydration
     * from snapshot data chunks is handled by a separate import pipeline (not
     * yet wired). The checkpoint is advanced here so that block sync resumes
     * from snapshotHeight+1.
     */
    static PersistentSyncApplyResult importSnapshot(
        const PersistentSyncCheckpoint& checkpoint,
        const PersistentSnapshotSyncManifest& manifest,
        NodeRuntime& runtime,
        const NodeDataDirectoryConfig& directoryConfig,
        PersistentSyncCheckpointStore* checkpointStore,
        std::int64_t now
    );

    /*
     * Full-protocol overload: deserializes each block, validates it against
     * the local blockchain using BlockValidationMode::ProtocolCommitment
     * (stateRoot and receiptsRoot are recomputed and compared to declared
     * values), then adds accepted blocks to `blockchain`.
     *
     * `contextBuilder` is called before each block with the current blockchain
     * state and must return an authoritative protocol context so the computed
     * roots reflect all previously applied blocks.
     *
     * Validation runs against a staged Blockchain copy. No block is added and
     * no checkpoint is advanced if any item, QC, or protocol commitment fails.
     */
    static PersistentSyncApplyResult applyValidatedBatch(
        const PersistentSyncCheckpoint& checkpoint,
        const PersistentBlockSyncBatch& batch,
        core::Blockchain& blockchain,
        const core::ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::function<core::StateTransitionPreviewContext(const core::Blockchain&)> contextBuilder,
        std::int64_t now,
        const core::ValidatorSetHistory* validatorSetHistory = nullptr
    );

};

class PersistentBlockStateSyncCodec {
public:
    static std::vector<unsigned char> encodeCheckpoint(
        const PersistentSyncCheckpoint& checkpoint
    );

    static PersistentSyncCheckpoint decodeCheckpoint(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashCheckpoint(
        const PersistentSyncCheckpoint& checkpoint
    );

    static std::vector<unsigned char> encodeBlockSyncItem(
        const PersistentBlockSyncItem& item
    );

    static PersistentBlockSyncItem decodeBlockSyncItem(
        const std::vector<unsigned char>& bytes
    );

    static std::vector<unsigned char> encodeBlockSyncBatch(
        const PersistentBlockSyncBatch& batch
    );

    static PersistentBlockSyncBatch decodeBlockSyncBatch(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashBlockSyncBatch(
        const PersistentBlockSyncBatch& batch
    );

    static std::vector<unsigned char> encodeSnapshotManifest(
        const PersistentSnapshotSyncManifest& manifest
    );

    static PersistentSnapshotSyncManifest decodeSnapshotManifest(
        const std::vector<unsigned char>& bytes
    );

    static std::string hashSnapshotManifest(
        const PersistentSnapshotSyncManifest& manifest
    );
};

} // namespace nodo::node

#endif
