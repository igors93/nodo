#include "p2p/PeerMessage.hpp"

#include "crypto/hash.h"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::p2p {

namespace {

constexpr std::int64_t DEFAULT_TTL_SECONDS = 120;
constexpr std::size_t MAX_PEER_MESSAGE_PAYLOAD_BYTES = 1024 * 1024;

bool isSafeIdentifier(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string hashString(
    const std::string& value
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    nodo_hash_string(
        value.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

} // namespace

PeerInfo::PeerInfo()
    : m_peerId(""),
      m_endpoint(""),
      m_protocolVersion(""),
      m_latestKnownHeight(0),
      m_lastSeenAt(0) {}

PeerInfo::PeerInfo(
    std::string peerId,
    std::string endpoint,
    std::string protocolVersion,
    std::uint64_t latestKnownHeight,
    std::int64_t lastSeenAt
)
    : m_peerId(std::move(peerId)),
      m_endpoint(std::move(endpoint)),
      m_protocolVersion(std::move(protocolVersion)),
      m_latestKnownHeight(latestKnownHeight),
      m_lastSeenAt(lastSeenAt) {}

const std::string& PeerInfo::peerId() const {
    return m_peerId;
}

const std::string& PeerInfo::endpoint() const {
    return m_endpoint;
}

const std::string& PeerInfo::protocolVersion() const {
    return m_protocolVersion;
}

std::uint64_t PeerInfo::latestKnownHeight() const {
    return m_latestKnownHeight;
}

std::int64_t PeerInfo::lastSeenAt() const {
    return m_lastSeenAt;
}

bool PeerInfo::isValid() const {
    return isSafeIdentifier(m_peerId) &&
           isSafeIdentifier(m_endpoint) &&
           isSafeIdentifier(m_protocolVersion) &&
           m_lastSeenAt > 0;
}

std::string PeerInfo::serialize() const {
    std::ostringstream oss;

    oss << "PeerInfo{"
        << "peerId=" << m_peerId
        << ";endpoint=" << m_endpoint
        << ";protocolVersion=" << m_protocolVersion
        << ";latestKnownHeight=" << m_latestKnownHeight
        << ";lastSeenAt=" << m_lastSeenAt
        << "}";

    return oss.str();
}

std::string peerMessageTypeToString(
    PeerMessageType type
) {
    switch (type) {
        case PeerMessageType::HANDSHAKE:
            return "HANDSHAKE";
        case PeerMessageType::CHAIN_SUMMARY:
            return "CHAIN_SUMMARY";
        case PeerMessageType::BLOCK_ANNOUNCEMENT:
            return "BLOCK_ANNOUNCEMENT";
        case PeerMessageType::BLOCK_REQUEST:
            return "BLOCK_REQUEST";
        case PeerMessageType::BLOCK_RESPONSE:
            return "BLOCK_RESPONSE";
        case PeerMessageType::TRANSACTION_BROADCAST:
            return "TRANSACTION_BROADCAST";
        case PeerMessageType::VOTE_BROADCAST:
            return "VOTE_BROADCAST";
        case PeerMessageType::QUORUM_CERTIFICATE_BROADCAST:
            return "QUORUM_CERTIFICATE_BROADCAST";
        case PeerMessageType::SYNC_REQUEST:
            return "SYNC_REQUEST";
        case PeerMessageType::SYNC_RESPONSE:
            return "SYNC_RESPONSE";
        case PeerMessageType::ERROR:
            return "ERROR";
        case PeerMessageType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

PeerMessage::PeerMessage()
    : m_messageId(""),
      m_type(PeerMessageType::UNKNOWN),
      m_fromPeerId(""),
      m_toPeerId(""),
      m_payload(""),
      m_createdAt(0),
      m_ttlSeconds(0) {}

PeerMessage::PeerMessage(
    std::string messageId,
    PeerMessageType type,
    std::string fromPeerId,
    std::string toPeerId,
    std::string payload,
    std::int64_t createdAt,
    std::int64_t ttlSeconds
)
    : m_messageId(std::move(messageId)),
      m_type(type),
      m_fromPeerId(std::move(fromPeerId)),
      m_toPeerId(std::move(toPeerId)),
      m_payload(std::move(payload)),
      m_createdAt(createdAt),
      m_ttlSeconds(ttlSeconds) {}

const std::string& PeerMessage::messageId() const {
    return m_messageId;
}

PeerMessageType PeerMessage::type() const {
    return m_type;
}

const std::string& PeerMessage::fromPeerId() const {
    return m_fromPeerId;
}

const std::string& PeerMessage::toPeerId() const {
    return m_toPeerId;
}

const std::string& PeerMessage::payload() const {
    return m_payload;
}

std::int64_t PeerMessage::createdAt() const {
    return m_createdAt;
}

std::int64_t PeerMessage::ttlSeconds() const {
    return m_ttlSeconds;
}

bool PeerMessage::expired(
    std::int64_t now
) const {
    if (now <= m_createdAt) {
        return false;
    }

    return now - m_createdAt > m_ttlSeconds;
}

bool PeerMessage::isValid() const {
    if (!isSafeIdentifier(m_messageId) ||
        !isSafeIdentifier(m_fromPeerId) ||
        !isSafeIdentifier(m_toPeerId)) {
        return false;
    }

    if (m_type == PeerMessageType::UNKNOWN ||
        m_payload.empty() ||
        m_payload.size() > MAX_PEER_MESSAGE_PAYLOAD_BYTES ||
        m_createdAt <= 0 ||
        m_ttlSeconds <= 0) {
        return false;
    }

    return m_messageId == computeMessageId(
        m_type,
        m_fromPeerId,
        m_toPeerId,
        m_payload,
        m_createdAt
    );
}

std::string PeerMessage::serialize() const {
    std::ostringstream oss;

    oss << "PeerMessage{"
        << "messageId=" << m_messageId
        << ";type=" << peerMessageTypeToString(m_type)
        << ";fromPeerId=" << m_fromPeerId
        << ";toPeerId=" << m_toPeerId
        << ";createdAt=" << m_createdAt
        << ";ttlSeconds=" << m_ttlSeconds
        << ";payload=" << m_payload
        << "}";

    return oss.str();
}

std::string PeerMessage::computeMessageId(
    PeerMessageType type,
    const std::string& fromPeerId,
    const std::string& toPeerId,
    const std::string& payload,
    std::int64_t createdAt
) {
    std::ostringstream oss;

    oss << "PeerMessageId{"
        << "type=" << peerMessageTypeToString(type)
        << ";fromPeerId=" << fromPeerId
        << ";toPeerId=" << toPeerId
        << ";createdAt=" << createdAt
        << ";payload=" << payload
        << "}";

    return hashString(oss.str());
}

PeerMessage PeerMessageFactory::handshake(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    std::int64_t createdAt
) {
    return create(
        PeerMessageType::HANDSHAKE,
        fromPeer,
        toPeerId,
        fromPeer.serialize(),
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::chainSummary(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    const consensus::ChainForkSummary& summary,
    std::int64_t createdAt
) {
    return create(
        PeerMessageType::CHAIN_SUMMARY,
        fromPeer,
        toPeerId,
        summary.serialize(),
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::blockAnnouncement(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    const core::Block& block,
    std::int64_t createdAt
) {
    return create(
        PeerMessageType::BLOCK_ANNOUNCEMENT,
        fromPeer,
        toPeerId,
        block.serialize(),
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::blockRequest(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    std::uint64_t fromHeight,
    std::uint64_t toHeight,
    std::int64_t createdAt
) {
    std::ostringstream payload;

    payload << "BlockRequest{"
            << "fromHeight=" << fromHeight
            << ";toHeight=" << toHeight
            << "}";

    return create(
        PeerMessageType::BLOCK_REQUEST,
        fromPeer,
        toPeerId,
        payload.str(),
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::syncRequest(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    const consensus::ChainForkSummary& localSummary,
    std::uint64_t fromHeight,
    std::uint64_t toHeight,
    std::int64_t createdAt
) {
    std::ostringstream payload;

    payload << "SyncRequest{"
            << "fromHeight=" << fromHeight
            << ";toHeight=" << toHeight
            << ";localSummary=" << localSummary.serialize()
            << "}";

    return create(
        PeerMessageType::SYNC_REQUEST,
        fromPeer,
        toPeerId,
        payload.str(),
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::error(
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    const std::string& reason,
    std::int64_t createdAt
) {
    return create(
        PeerMessageType::ERROR,
        fromPeer,
        toPeerId,
        "PeerError{reason=" + reason + "}",
        createdAt,
        DEFAULT_TTL_SECONDS
    );
}

PeerMessage PeerMessageFactory::create(
    PeerMessageType type,
    const PeerInfo& fromPeer,
    const std::string& toPeerId,
    const std::string& payload,
    std::int64_t createdAt,
    std::int64_t ttlSeconds
) {
    if (!fromPeer.isValid() ||
        !isSafeIdentifier(toPeerId) ||
        payload.empty() ||
        createdAt <= 0 ||
        ttlSeconds <= 0) {
        throw std::invalid_argument("Invalid peer message creation input.");
    }

    const std::string messageId =
        PeerMessage::computeMessageId(
            type,
            fromPeer.peerId(),
            toPeerId,
            payload,
            createdAt
        );

    return PeerMessage(
        messageId,
        type,
        fromPeer.peerId(),
        toPeerId,
        payload,
        createdAt,
        ttlSeconds
    );
}

} // namespace nodo::p2p
