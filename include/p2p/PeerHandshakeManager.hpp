#ifndef NODO_P2P_PEER_HANDSHAKE_MANAGER_HPP
#define NODO_P2P_PEER_HANDSHAKE_MANAGER_HPP

#include "node/ChainSyncMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/Peer.hpp"

#include <cstdint>
#include <string>

namespace nodo::p2p {

class PeerHelloMessage {
public:
    PeerHelloMessage();

    PeerHelloMessage(
        PeerMetadata peer,
        std::string networkId,
        std::string chainId,
        std::string protocolVersion,
        node::ChainStatusMessage chainStatus,
        std::int64_t createdAt
    );

    const PeerMetadata& peer() const;
    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    const node::ChainStatusMessage& chainStatus() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    PeerMetadata m_peer;
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    node::ChainStatusMessage m_chainStatus;
    std::int64_t m_createdAt;
};

enum class PeerHandshakeStatus {
    ACCEPTED,
    REJECTED
};

class PeerHandshakeResult {
public:
    PeerHandshakeResult();
    PeerHandshakeResult(PeerHandshakeStatus status, std::string reason);

    PeerHandshakeStatus status() const;
    const std::string& reason() const;
    bool accepted() const;
    std::string serialize() const;

private:
    PeerHandshakeStatus m_status;
    std::string m_reason;
};

class PeerHandshakeManager {
public:
    static NetworkEnvelope createHelloEnvelope(
        const GossipMeshConfig& config,
        const PeerMetadata& localPeer,
        const node::ChainStatusMessage& chainStatus,
        std::int64_t now
    );

    static PeerHandshakeResult validateHello(
        const GossipMeshConfig& config,
        const NetworkEnvelope& envelope,
        std::int64_t now
    );
};

} // namespace nodo::p2p

#endif
