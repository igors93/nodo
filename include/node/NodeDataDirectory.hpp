#ifndef NODO_NODE_NODE_DATA_DIRECTORY_HPP
#define NODO_NODE_NODE_DATA_DIRECTORY_HPP

#include "config/NetworkParameters.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/PeerMessage.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace nodo::node {

/*
 * NodeDataDirectoryConfig describes where a local node stores its durable
 * runtime files.
 *
 * Security principle:
 * Persistence code must never guess network identity from an arbitrary folder.
 * The manifest stores the genesis id and network parameters so operators can
 * inspect what a node was initialized for.
 */
class NodeDataDirectoryConfig {
public:
    NodeDataDirectoryConfig();

    explicit NodeDataDirectoryConfig(
        std::filesystem::path rootPath
    );

    const std::filesystem::path& rootPath() const;

    std::filesystem::path manifestPath() const;
    std::filesystem::path genesisConfigPath() const;
    std::filesystem::path localPeerPath() const;
    std::filesystem::path runtimeSnapshotPath() const;
    std::filesystem::path blocksDirectoryPath() const;
    std::filesystem::path peersDirectoryPath() const;
    std::filesystem::path mempoolDirectoryPath() const;
    std::filesystem::path runtimeDirectoryPath() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::filesystem::path m_rootPath;
};

/*
 * NodeRuntimeManifest is the durable summary of a local node directory.
 *
 * It intentionally stores only auditable summary fields. Full block persistence
 * can be added later without changing the manifest identity model.
 */
class NodeRuntimeManifest {
public:
    NodeRuntimeManifest();

    NodeRuntimeManifest(
        std::string chainId,
        std::string networkName,
        std::string protocolVersion,
        std::string genesisConfigId,
        std::uint64_t latestBlockHeight,
        std::string latestBlockHash,
        std::size_t validatorCount,
        std::size_t peerCount,
        std::int64_t createdAt,
        std::int64_t updatedAt
    );

    const std::string& chainId() const;
    const std::string& networkName() const;
    const std::string& protocolVersion() const;
    const std::string& genesisConfigId() const;
    std::uint64_t latestBlockHeight() const;
    const std::string& latestBlockHash() const;
    std::size_t validatorCount() const;
    std::size_t peerCount() const;
    std::int64_t createdAt() const;
    std::int64_t updatedAt() const;

    bool isValid() const;

    std::string serialize() const;
    std::string toFileContents() const;

    static NodeRuntimeManifest fromFileContents(
        const std::string& contents
    );

    static NodeRuntimeManifest fromRuntime(
        const NodeRuntime& runtime,
        std::int64_t createdAt,
        std::int64_t updatedAt
    );

private:
    std::string m_chainId;
    std::string m_networkName;
    std::string m_protocolVersion;
    std::string m_genesisConfigId;
    std::uint64_t m_latestBlockHeight;
    std::string m_latestBlockHash;
    std::size_t m_validatorCount;
    std::size_t m_peerCount;
    std::int64_t m_createdAt;
    std::int64_t m_updatedAt;
};

enum class NodeDataDirectoryInitStatus {
    INITIALIZED,
    ALREADY_INITIALIZED,
    INVALID_CONFIG,
    INVALID_GENESIS_CONFIG,
    INVALID_LOCAL_PEER,
    GENESIS_BUILD_FAILED,
    EXISTS_WITH_DIFFERENT_GENESIS,
    IO_ERROR
};

std::string nodeDataDirectoryInitStatusToString(
    NodeDataDirectoryInitStatus status
);

class NodeDataDirectoryInitResult {
public:
    NodeDataDirectoryInitResult();

    static NodeDataDirectoryInitResult initialized(
        NodeRuntimeManifest manifest
    );

    static NodeDataDirectoryInitResult alreadyInitialized(
        NodeRuntimeManifest manifest
    );

    static NodeDataDirectoryInitResult rejected(
        NodeDataDirectoryInitStatus status,
        std::string reason
    );

    NodeDataDirectoryInitStatus status() const;
    const std::string& reason() const;
    const NodeRuntimeManifest& manifest() const;

    bool success() const;
    bool initialized() const;
    bool alreadyInitialized() const;

    std::string serialize() const;

private:
    NodeDataDirectoryInitStatus m_status;
    std::string m_reason;
    NodeRuntimeManifest m_manifest;
};

enum class NodeDataDirectoryReadStatus {
    LOADED,
    INVALID_CONFIG,
    NOT_INITIALIZED,
    INVALID_MANIFEST,
    IO_ERROR
};

std::string nodeDataDirectoryReadStatusToString(
    NodeDataDirectoryReadStatus status
);

class NodeDataDirectoryReadResult {
public:
    NodeDataDirectoryReadResult();

    static NodeDataDirectoryReadResult loaded(
        NodeRuntimeManifest manifest
    );

    static NodeDataDirectoryReadResult rejected(
        NodeDataDirectoryReadStatus status,
        std::string reason
    );

    NodeDataDirectoryReadStatus status() const;
    const std::string& reason() const;
    const NodeRuntimeManifest& manifest() const;

    bool loaded() const;
    std::string serialize() const;

private:
    NodeDataDirectoryReadStatus m_status;
    std::string m_reason;
    NodeRuntimeManifest m_manifest;
};

class NodeDataDirectory {
public:
    static NodeDataDirectoryInitResult initialize(
        const NodeDataDirectoryConfig& directoryConfig,
        const config::GenesisConfig& genesisConfig,
        const p2p::PeerInfo& localPeer,
        std::int64_t createdAt
    );

    static NodeDataDirectoryReadResult loadManifest(
        const NodeDataDirectoryConfig& directoryConfig
    );

    static NodeDataDirectoryReadResult writeRuntimeSnapshot(
        const NodeDataDirectoryConfig& directoryConfig,
        const NodeRuntime& runtime,
        std::int64_t updatedAt
    );

    static bool isInitialized(
        const NodeDataDirectoryConfig& directoryConfig
    );

private:
    static void ensureDirectoryTree(
        const NodeDataDirectoryConfig& directoryConfig
    );

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
