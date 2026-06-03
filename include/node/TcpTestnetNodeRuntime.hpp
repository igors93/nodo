#ifndef NODO_NODE_TCP_TESTNET_NODE_RUNTIME_HPP
#define NODO_NODE_TCP_TESTNET_NODE_RUNTIME_HPP

#include "node/ChainSyncMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/TcpTransport.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodo::node {

class TcpTestnetNodeRuntimeConfig {
public:
    TcpTestnetNodeRuntimeConfig();

    TcpTestnetNodeRuntimeConfig(
        std::string nodeId,
        std::string host,
        std::uint16_t port,
        std::string networkId,
        std::string chainId,
        std::string protocolVersion,
        std::string genesisId,
        std::filesystem::path dataDirectory,
        std::uint32_t defaultTtlSeconds,
        std::size_t invalidMessageQuarantineThreshold
    );

    const std::string& nodeId() const;
    const std::string& host() const;
    std::uint16_t port() const;
    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    const std::string& genesisId() const;
    const std::filesystem::path& dataDirectory() const;
    std::uint32_t defaultTtlSeconds() const;
    std::size_t invalidMessageQuarantineThreshold() const;

    std::filesystem::path peersFilePath() const;
    bool isValid() const;

private:
    std::string m_nodeId;
    std::string m_host;
    std::uint16_t m_port;
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::string m_genesisId;
    std::filesystem::path m_dataDirectory;
    std::uint32_t m_defaultTtlSeconds;
    std::size_t m_invalidMessageQuarantineThreshold;
};

class TcpTestnetPeerFileEntry {
public:
    TcpTestnetPeerFileEntry();

    TcpTestnetPeerFileEntry(
        std::string nodeId,
        p2p::PeerEndpoint endpoint,
        std::string publicKeyFingerprint
    );

    const std::string& nodeId() const;
    const p2p::PeerEndpoint& endpoint() const;
    const std::string& publicKeyFingerprint() const;

    bool isValid() const;
    std::string serialize() const;

    static TcpTestnetPeerFileEntry parseLine(
        const std::string& line
    );

private:
    std::string m_nodeId;
    p2p::PeerEndpoint m_endpoint;
    std::string m_publicKeyFingerprint;
};

class TcpTestnetPeerStore {
public:
    static std::vector<TcpTestnetPeerFileEntry> load(
        const std::filesystem::path& peersFile
    );

    static void save(
        const std::filesystem::path& peersFile,
        const std::vector<TcpTestnetPeerFileEntry>& peers
    );
};

class TcpTestnetNodeRuntime {
public:
    explicit TcpTestnetNodeRuntime(
        TcpTestnetNodeRuntimeConfig config
    );

    const TcpTestnetNodeRuntimeConfig& config() const;
    p2p::TcpTransport& transport();
    const p2p::TcpTransport& transport() const;
    p2p::GossipMesh& gossipMesh();
    const p2p::GossipMesh& gossipMesh() const;

    bool running() const;

    p2p::TransportResult start();
    void stop();

    p2p::PeerRegistryResult addPeer(
        const p2p::PeerMetadata& peer
    );

    std::size_t loadPeersFromDisk(
        std::int64_t now
    );

    void savePeersToDisk() const;

    p2p::TransportResult connectPeer(
        const std::string& remoteNodeId
    );

    p2p::GossipDeliveryReport broadcast(
        p2p::NetworkMessageType type,
        const std::string& payload,
        std::int64_t now
    );

    p2p::GossipDeliveryReport broadcastChainStatus(
        const ChainStatusMessage& chainStatus,
        std::int64_t now
    );

    p2p::GossipDeliveryReport tick(
        std::int64_t now
    );

private:
    TcpTestnetNodeRuntimeConfig m_config;
    p2p::TcpTransport m_transport;
    p2p::GossipMesh m_gossipMesh;
    bool m_running;

    p2p::GossipMeshConfig makeGossipConfig() const;
};

} // namespace nodo::node

#endif
