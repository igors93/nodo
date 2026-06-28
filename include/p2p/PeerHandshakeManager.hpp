#ifndef NODO_P2P_PEER_HANDSHAKE_MANAGER_HPP
#define NODO_P2P_PEER_HANDSHAKE_MANAGER_HPP

#include "node/ChainSyncMessages.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/Peer.hpp"

#include <cstdint>
#include <optional>
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
        std::string genesisId,
        node::ChainStatusMessage chainStatus,
        crypto::SignatureBundle identityProof,
        std::int64_t createdAt
    );

    const PeerMetadata& peer() const;
    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    const std::string& genesisId() const;
    const node::ChainStatusMessage& chainStatus() const;
    const crypto::SignatureBundle& identityProof() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string signingPayload() const;
    std::string serialize() const;

private:
    PeerMetadata m_peer;
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::string m_genesisId;
    node::ChainStatusMessage m_chainStatus;
    crypto::SignatureBundle m_identityProof;
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
        const crypto::KeyPair& nodeIdentityKey,
        std::int64_t now
    );

    static PeerHandshakeResult validateHello(
        const GossipMeshConfig& config,
        const NetworkEnvelope& envelope,
        std::int64_t now
    );

    static std::optional<PeerMetadata> peerMetadataFromHello(
        const NetworkEnvelope& envelope,
        std::int64_t now
    );
};

} // namespace nodo::p2p

#endif
