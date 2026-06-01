#ifndef NODO_NODE_RUNTIME_STATE_LOADER_HPP
#define NODO_NODE_RUNTIME_STATE_LOADER_HPP

#include "config/NetworkParameters.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/PeerMessage.hpp"

#include <cstddef>
#include <string>

namespace nodo::node {

enum class RuntimeStateLoadStatus {
    LOADED,
    INVALID_CONFIG,
    NOT_INITIALIZED,
    GENESIS_MISMATCH,
    RUNTIME_START_FAILED,
    BLOCK_FILE_MISSING,
    BLOCK_FILE_INVALID,
    BLOCK_APPEND_FAILED,
    MANIFEST_MISMATCH,
    MEMPOOL_LOAD_FAILED
};

std::string runtimeStateLoadStatusToString(
    RuntimeStateLoadStatus status
);

class RuntimeStateLoadResult {
public:
    RuntimeStateLoadResult();

    static RuntimeStateLoadResult loaded(
        NodeRuntime runtime,
        NodeRuntimeManifest manifest,
        std::size_t loadedBlockCount,
        std::size_t loadedMempoolTransactionCount
    );

    static RuntimeStateLoadResult rejected(
        RuntimeStateLoadStatus status,
        std::string reason
    );

    RuntimeStateLoadStatus status() const;
    const std::string& reason() const;
    bool loaded() const;

    const NodeRuntime& runtime() const;
    const NodeRuntimeManifest& manifest() const;
    std::size_t loadedBlockCount() const;
    std::size_t loadedMempoolTransactionCount() const;

    std::string serialize() const;

private:
    RuntimeStateLoadStatus m_status;
    std::string m_reason;
    NodeRuntime m_runtime;
    NodeRuntimeManifest m_manifest;
    std::size_t m_loadedBlockCount;
    std::size_t m_loadedMempoolTransactionCount;
};

class RuntimeStateLoader {
public:
    static RuntimeStateLoadResult loadFromDataDirectory(
        const NodeDataDirectoryConfig& directoryConfig,
        const config::GenesisConfig& genesisConfig,
        const p2p::PeerInfo& localPeer
    );
};

} // namespace nodo::node

#endif
