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

class PeerChallengeMessage {
public:
    PeerChallengeMessage();

    PeerChallengeMessage(
        std::string challengerNodeId,
        std::string challengedNodeId,
        std::string nonce,
        std::string ephemeralPublicKeyHex,
        std::int64_t createdAt,
        std::uint32_t ttlSeconds
    );

    const std::string& challengerNodeId() const;
    const std::string& challengedNodeId() const;
    const std::string& nonce() const;
    const std::string& ephemeralPublicKeyHex() const;
    std::int64_t createdAt() const;
    std::uint32_t ttlSeconds() const;
    std::int64_t expiresAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_challengerNodeId;
    std::string m_challengedNodeId;
    std::string m_nonce;
    std::string m_ephemeralPublicKeyHex;
    std::int64_t m_createdAt;
    std::uint32_t m_ttlSeconds;
};

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
        std::string challengeIssuerNodeId,
        std::string challengeNonce,
        std::string challengeEphemeralPublicKeyHex,
        std::string ephemeralPublicKeyHex,
        crypto::SignatureBundle identityProof,
        std::int64_t createdAt
    );

    const PeerMetadata& peer() const;
    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    const std::string& genesisId() const;
    const node::ChainStatusMessage& chainStatus() const;
    const std::string& challengeIssuerNodeId() const;
    const std::string& challengeNonce() const;
    const std::string& challengeEphemeralPublicKeyHex() const;
    const std::string& ephemeralPublicKeyHex() const;
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
    std::string m_challengeIssuerNodeId;
    std::string m_challengeNonce;
    std::string m_challengeEphemeralPublicKeyHex;
    std::string m_ephemeralPublicKeyHex;
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
    static NetworkEnvelope createChallengeEnvelope(
        const GossipMeshConfig& config,
        const std::string& challengedNodeId,
        const std::string& nonce,
        std::int64_t now
    );

    static NetworkEnvelope createChallengeEnvelope(
        const GossipMeshConfig& config,
        const std::string& challengedNodeId,
        const std::string& nonce,
        const std::string& ephemeralPublicKeyHex,
        std::int64_t now
    );

    static std::optional<PeerChallengeMessage> challengeFromEnvelope(
        const GossipMeshConfig& config,
        const NetworkEnvelope& envelope,
        std::int64_t now
    );

    static NetworkEnvelope createHelloEnvelope(
        const GossipMeshConfig& config,
        const PeerMetadata& localPeer,
        const node::ChainStatusMessage& chainStatus,
        const std::string& challengeIssuerNodeId,
        const std::string& challengeNonce,
        const crypto::KeyPair& nodeIdentityKey,
        std::int64_t now
    );

    static NetworkEnvelope createHelloEnvelope(
        const GossipMeshConfig& config,
        const PeerMetadata& localPeer,
        const node::ChainStatusMessage& chainStatus,
        const std::string& challengeIssuerNodeId,
        const std::string& challengeNonce,
        const std::string& challengeEphemeralPublicKeyHex,
        const std::string& ephemeralPublicKeyHex,
        const crypto::KeyPair& nodeIdentityKey,
        std::int64_t now
    );

    static PeerHandshakeResult validateHello(
        const GossipMeshConfig& config,
        const NetworkEnvelope& envelope,
        const std::string& expectedChallengeNonce,
        std::int64_t now
    );

    static PeerHandshakeResult validateHello(
        const GossipMeshConfig& config,
        const NetworkEnvelope& envelope,
        const std::string& expectedChallengeNonce,
        const std::string& expectedChallengeEphemeralPublicKeyHex,
        std::int64_t now
    );

    static std::optional<PeerMetadata> peerMetadataFromHello(
        const NetworkEnvelope& envelope,
        std::int64_t now
    );

    static std::string challengeNonceFromHello(
        const NetworkEnvelope& envelope
    );

    static std::string ephemeralPublicKeyFromHello(
        const NetworkEnvelope& envelope
    );
};

} // namespace nodo::p2p

#endif
