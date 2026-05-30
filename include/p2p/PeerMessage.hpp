#ifndef NODO_P2P_PEER_MESSAGE_HPP
#define NODO_P2P_PEER_MESSAGE_HPP

#include "consensus/ForkChoice.hpp"
#include "core/Block.hpp"

#include <cstdint>
#include <string>

namespace nodo::p2p {

class PeerInfo {
public:
    PeerInfo();

    PeerInfo(
        std::string peerId,
        std::string endpoint,
        std::string protocolVersion,
        std::uint64_t latestKnownHeight,
        std::int64_t lastSeenAt
    );

    const std::string& peerId() const;
    const std::string& endpoint() const;
    const std::string& protocolVersion() const;
    std::uint64_t latestKnownHeight() const;
    std::int64_t lastSeenAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_peerId;
    std::string m_endpoint;
    std::string m_protocolVersion;
    std::uint64_t m_latestKnownHeight;
    std::int64_t m_lastSeenAt;
};

enum class PeerMessageType {
    UNKNOWN,
    HANDSHAKE,
    CHAIN_SUMMARY,
    BLOCK_ANNOUNCEMENT,
    BLOCK_REQUEST,
    BLOCK_RESPONSE,
    TRANSACTION_BROADCAST,
    VOTE_BROADCAST,
    QUORUM_CERTIFICATE_BROADCAST,
    SYNC_REQUEST,
    SYNC_RESPONSE,
    ERROR
};

std::string peerMessageTypeToString(
    PeerMessageType type
);

/*
 * PeerMessage is an envelope for deterministic P2P messages.
 *
 * This phase does not open sockets yet. It defines safe message types and a
 * deterministic envelope that future transport code can send over TCP/QUIC/etc.
 */
class PeerMessage {
public:
    PeerMessage();

    PeerMessage(
        std::string messageId,
        PeerMessageType type,
        std::string fromPeerId,
        std::string toPeerId,
        std::string payload,
        std::int64_t createdAt,
        std::int64_t ttlSeconds
    );

    const std::string& messageId() const;
    PeerMessageType type() const;
    const std::string& fromPeerId() const;
    const std::string& toPeerId() const;
    const std::string& payload() const;
    std::int64_t createdAt() const;
    std::int64_t ttlSeconds() const;

    bool expired(
        std::int64_t now
    ) const;

    bool isValid() const;

    std::string serialize() const;

    static std::string computeMessageId(
        PeerMessageType type,
        const std::string& fromPeerId,
        const std::string& toPeerId,
        const std::string& payload,
        std::int64_t createdAt
    );

private:
    std::string m_messageId;
    PeerMessageType m_type;
    std::string m_fromPeerId;
    std::string m_toPeerId;
    std::string m_payload;
    std::int64_t m_createdAt;
    std::int64_t m_ttlSeconds;
};

class PeerMessageFactory {
public:
    static PeerMessage handshake(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        std::int64_t createdAt
    );

    static PeerMessage chainSummary(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        const consensus::ChainForkSummary& summary,
        std::int64_t createdAt
    );

    static PeerMessage blockAnnouncement(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        const core::Block& block,
        std::int64_t createdAt
    );

    static PeerMessage blockRequest(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        std::uint64_t fromHeight,
        std::uint64_t toHeight,
        std::int64_t createdAt
    );

    static PeerMessage syncRequest(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        const consensus::ChainForkSummary& localSummary,
        std::uint64_t fromHeight,
        std::uint64_t toHeight,
        std::int64_t createdAt
    );

    static PeerMessage error(
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        const std::string& reason,
        std::int64_t createdAt
    );

private:
    static PeerMessage create(
        PeerMessageType type,
        const PeerInfo& fromPeer,
        const std::string& toPeerId,
        const std::string& payload,
        std::int64_t createdAt,
        std::int64_t ttlSeconds
    );
};

} // namespace nodo::p2p

#endif
