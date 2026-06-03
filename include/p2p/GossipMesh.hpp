#ifndef NODO_P2P_GOSSIP_MESH_HPP
#define NODO_P2P_GOSSIP_MESH_HPP

#include "node/EvidenceCaptureHealth.hpp"
#include "p2p/InboundMessageValidator.hpp"
#include "p2p/OutboundMessageQueue.hpp"
#include "p2p/PeerRegistry.hpp"
#include "p2p/PeerRateLimiter.hpp"
#include "p2p/Transport.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nodo::storage {
class ProtocolEvidenceStore;
} // namespace nodo::storage

namespace nodo::p2p {

class GossipMeshConfig {
public:
    GossipMeshConfig();

    GossipMeshConfig(
        std::string localNodeId,
        std::string networkId,
        std::string chainId,
        std::string protocolVersion,
        std::string genesisId,
        std::uint32_t defaultTtlSeconds,
        std::size_t invalidMessageQuarantineThreshold
    );

    const std::string& localNodeId() const;
    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    const std::string& genesisId() const;
    std::uint32_t defaultTtlSeconds() const;
    std::size_t invalidMessageQuarantineThreshold() const;

    bool isValid() const;

private:
    std::string m_localNodeId;
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::string m_genesisId;
    std::uint32_t m_defaultTtlSeconds;
    std::size_t m_invalidMessageQuarantineThreshold;
};

class GossipDeliveryReport {
public:
    GossipDeliveryReport();
    GossipDeliveryReport(std::size_t acceptedCount, std::size_t rejectedCount);

    std::size_t acceptedCount() const;
    std::size_t rejectedCount() const;
    bool allAccepted() const;
    std::string serialize() const;

private:
    std::size_t m_acceptedCount;
    std::size_t m_rejectedCount;
};

class GossipInbox {
public:
    GossipInbox();

    void add(const NetworkEnvelope& envelope);

    std::size_t totalCount() const;
    std::size_t countForType(NetworkMessageType type) const;
    std::vector<NetworkEnvelope> messagesForType(NetworkMessageType type) const;
    std::string serialize() const;

private:
    std::map<NetworkMessageType, std::vector<NetworkEnvelope>> m_messagesByType;
};

class GossipMesh {
public:
    GossipMesh(
        GossipMeshConfig config,
        Transport& transport
    );

    // Optional overload: wire in an evidence store to persist protocol violations.
    GossipMesh(
        GossipMeshConfig config,
        Transport& transport,
        storage::ProtocolEvidenceStore* evidenceStore
    );

    const GossipMeshConfig& config() const;
    PeerRegistry& peerRegistry();
    const PeerRegistry& peerRegistry() const;
    const GossipInbox& inbox() const;

    PeerRegistryResult registerPeer(PeerMetadata peer);

    TransportResult connectPeer(
        const std::string& remoteNodeId
    );

    TransportResult disconnectPeer(
        const std::string& remoteNodeId
    );

    NetworkEnvelope createEnvelope(
        NetworkMessageType type,
        const std::string& payload,
        std::int64_t now
    ) const;

    GossipDeliveryReport broadcast(
        NetworkMessageType type,
        const std::string& payload,
        std::int64_t now
    );

    GossipDeliveryReport flushOutbound(
        std::int64_t now
    );

    GossipDeliveryReport receiveAvailable(
        std::int64_t now
    );

    std::size_t invalidMessageCountForPeer(
        const std::string& nodeId
    ) const;

    std::uint32_t rateLimitedMessageCountForPeer(
        const std::string& nodeId,
        std::int64_t now
    ) const;

    // Returns a snapshot of the current evidence capture health.
    // Use this to surface persistence failures through operator diagnostics.
    const node::EvidenceCaptureHealth& evidenceCaptureHealth() const;

private:
    GossipMeshConfig m_config;
    Transport& m_transport;
    storage::ProtocolEvidenceStore* m_evidenceStore;
    PeerRegistry m_peerRegistry;
    OutboundMessageQueue m_outboundQueue;
    InboundMessageValidator m_inboundValidator;
    PeerRateLimiter m_rateLimiter;
    GossipInbox m_inbox;
    std::map<std::string, std::size_t> m_invalidMessagesByPeer;
    // Coalescing: tracks (nodeId, ruleId) -> last evidence timestamp (seconds).
    std::map<std::pair<std::string, std::string>, std::int64_t> m_lastEvidenceAt;
    node::EvidenceCaptureHealth m_evidenceCaptureHealth;

    bool shouldQuarantinePeer(
        const std::string& nodeId
    ) const;

    void recordInvalidMessage(
        const std::string& nodeId,
        const std::string& reason,
        std::int64_t now = 0
    );
};

} // namespace nodo::p2p

#endif
