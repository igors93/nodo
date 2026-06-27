#ifndef NODO_NODE_FINALIZED_BLOCK_STORE_HPP
#define NODO_NODE_FINALIZED_BLOCK_STORE_HPP

#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeBlockPipeline.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::node {

enum class FinalizedBlockStoreStatus {
    STORED,
    ALREADY_STORED,
    INVALID_CONFIG,
    INVALID_RUNTIME,
    INVALID_PIPELINE_RESULT,
    NOT_INITIALIZED,
    CONFLICTING_BLOCK_FILE,
    IO_ERROR
};

std::string finalizedBlockStoreStatusToString(
    FinalizedBlockStoreStatus status
);

class FinalizedBlockStoreResult {
public:
    FinalizedBlockStoreResult();

    static FinalizedBlockStoreResult stored(
        NodeRuntimeManifest manifest,
        std::filesystem::path blockPath
    );

    static FinalizedBlockStoreResult alreadyStored(
        NodeRuntimeManifest manifest,
        std::filesystem::path blockPath
    );

    static FinalizedBlockStoreResult rejected(
        FinalizedBlockStoreStatus status,
        std::string reason
    );

    FinalizedBlockStoreStatus status() const;
    const std::string& reason() const;
    const NodeRuntimeManifest& manifest() const;
    const std::filesystem::path& blockPath() const;

    bool stored() const;
    bool alreadyStored() const;
    bool success() const;

    std::string serialize() const;

private:
    FinalizedBlockStoreStatus m_status;
    std::string m_reason;
    NodeRuntimeManifest m_manifest;
    std::filesystem::path m_blockPath;
};

/*
 * FinalizedBlockStore writes a finalized block artifact and then updates the
 * runtime manifest.
 *
 * The write order is intentional:
 *   1. write block artifact
 *   2. update runtime snapshot and manifest
 *
 * This avoids claiming a new height in the manifest before the block artifact
 * exists on disk.
 */
class FinalizedBlockStore {
public:
    /* Completes or rolls back cleanup for a commit interrupted by a crash. */
    static void recoverInterruptedCommit(
        const NodeDataDirectoryConfig& directoryConfig
    );

    static FinalizedBlockStoreResult persist(
        const NodeDataDirectoryConfig& directoryConfig,
        const NodeRuntime& runtime,
        const RuntimeBlockPipelineResult& pipelineResult,
        std::int64_t updatedAt
    );

    static FinalizedBlockStoreResult persistBatch(
        const NodeDataDirectoryConfig& directoryConfig,
        const NodeRuntime& finalRuntime,
        const std::vector<RuntimeBlockPipelineResult>& results,
        std::int64_t updatedAt
    );

    static std::filesystem::path blockFilePath(
        const NodeDataDirectoryConfig& directoryConfig,
        std::uint64_t blockIndex
    );

    static std::string finalizedBlockFileContents(
        const RuntimeBlockPipelineResult& pipelineResult
    );

    static std::filesystem::path commitJournalPath(
        const NodeDataDirectoryConfig& directoryConfig
    );

private:
    static void writeTextFile(
        const std::filesystem::path& path,
        const std::string& contents
    );

    static std::string readTextFile(
        const std::filesystem::path& path
    );
};

} // namespace nodo::node

#endif
