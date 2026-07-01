#ifndef NODO_P2P_PEER_HPP
#define NODO_P2P_PEER_HPP

#include <cstdint>
#include <string>

namespace nodo::p2p {

class PeerEndpoint {
public:
    PeerEndpoint();
    PeerEndpoint(std::string host, std::uint16_t port);

    const std::string& host() const;
    std::uint16_t port() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_host;
    std::uint16_t m_port;
};

class PeerMetadata {
public:
    PeerMetadata();

    PeerMetadata(
        std::string nodeId,
        PeerEndpoint endpoint,
        std::string publicKeyFingerprint,
        std::int64_t firstSeenAt,
        std::int64_t lastSeenAt,
        std::int32_t score,
        bool quarantined
    );

    PeerMetadata(
        std::string nodeId,
        PeerEndpoint endpoint,
        std::string publicKeyFingerprint,
        std::int64_t firstSeenAt,
        std::int64_t lastSeenAt,
        std::int32_t score,
        bool quarantined,
        std::int64_t bannedUntil,
        std::string banReason
    );

    const std::string& nodeId() const;
    const PeerEndpoint& endpoint() const;
    const std::string& publicKeyFingerprint() const;
    std::int64_t firstSeenAt() const;
    std::int64_t lastSeenAt() const;
    std::int32_t score() const;
    bool quarantined() const;
    std::int64_t bannedUntil() const;
    const std::string& banReason() const;
    bool bannedAt(std::int64_t now) const;

    // Stable reputation key derived from the cryptographic identity rather
    // than the operator-selected node id.
    std::string identityKey() const;

    PeerMetadata withHeartbeat(std::int64_t lastSeenAt) const;
    PeerMetadata withScoreDelta(std::int32_t delta) const;
    PeerMetadata quarantinedCopy() const;
    PeerMetadata bannedCopy(std::int64_t bannedUntil, std::string reason) const;
    PeerMetadata penaltyLiftedCopy(std::int64_t seenAt) const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_nodeId;
    PeerEndpoint m_endpoint;
    std::string m_publicKeyFingerprint;
    std::int64_t m_firstSeenAt;
    std::int64_t m_lastSeenAt;
    std::int32_t m_score;
    bool m_quarantined;
    std::int64_t m_bannedUntil;
    std::string m_banReason;
};

} // namespace nodo::p2p

#endif
