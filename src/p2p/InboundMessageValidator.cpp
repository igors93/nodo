#include "p2p/InboundMessageValidator.hpp"

#include <cstdlib>
#include <limits>
#include <utility>

namespace nodo::p2p {

InboundMessagePolicy::InboundMessagePolicy()
    : m_maxPayloadBytes(1024 * 1024),
      m_maxTtlSeconds(120),
      m_maxClockSkewSeconds(30),
      m_maxMessagesPerPeerWindow(1000) {}

InboundMessagePolicy::InboundMessagePolicy(
    std::size_t maxPayloadBytes,
    std::uint32_t maxTtlSeconds,
    std::int64_t maxClockSkewSeconds,
    std::size_t maxMessagesPerPeerWindow
) : m_maxPayloadBytes(maxPayloadBytes),
    m_maxTtlSeconds(maxTtlSeconds),
    m_maxClockSkewSeconds(maxClockSkewSeconds),
    m_maxMessagesPerPeerWindow(maxMessagesPerPeerWindow) {}

std::size_t InboundMessagePolicy::maxPayloadBytes() const { return m_maxPayloadBytes; }
std::uint32_t InboundMessagePolicy::maxTtlSeconds() const { return m_maxTtlSeconds; }
std::int64_t InboundMessagePolicy::maxClockSkewSeconds() const { return m_maxClockSkewSeconds; }
std::size_t InboundMessagePolicy::maxMessagesPerPeerWindow() const { return m_maxMessagesPerPeerWindow; }

bool InboundMessagePolicy::isValid() const {
    return m_maxPayloadBytes > 0 &&
           m_maxTtlSeconds > 0 &&
           m_maxClockSkewSeconds >= 0 &&
           m_maxMessagesPerPeerWindow > 0;
}

std::string inboundMessageStatusToString(InboundMessageStatus status) {
    switch (status) {
        case InboundMessageStatus::ACCEPTED: return "ACCEPTED";
        case InboundMessageStatus::INVALID_POLICY: return "INVALID_POLICY";
        case InboundMessageStatus::INVALID_ENVELOPE: return "INVALID_ENVELOPE";
        case InboundMessageStatus::WRONG_NETWORK: return "WRONG_NETWORK";
        case InboundMessageStatus::WRONG_CHAIN: return "WRONG_CHAIN";
        case InboundMessageStatus::WRONG_PROTOCOL: return "WRONG_PROTOCOL";
        case InboundMessageStatus::EXPIRED: return "EXPIRED";
        case InboundMessageStatus::TTL_TOO_LARGE: return "TTL_TOO_LARGE";
        case InboundMessageStatus::CLOCK_SKEW: return "CLOCK_SKEW";
        case InboundMessageStatus::DUPLICATE_MESSAGE: return "DUPLICATE_MESSAGE";
        case InboundMessageStatus::RATE_LIMITED: return "RATE_LIMITED";
        default: return "INVALID_ENVELOPE";
    }
}

InboundMessageResult::InboundMessageResult()
    : m_status(InboundMessageStatus::INVALID_ENVELOPE),
      m_reason("Uninitialized inbound message result.") {}

InboundMessageResult::InboundMessageResult(InboundMessageStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

InboundMessageStatus InboundMessageResult::status() const { return m_status; }
const std::string& InboundMessageResult::reason() const { return m_reason; }
bool InboundMessageResult::accepted() const { return m_status == InboundMessageStatus::ACCEPTED; }

InboundMessageValidator::InboundMessageValidator(InboundMessagePolicy policy)
    : m_policy(policy),
      m_seenMessageIds(),
      m_messagesByPeerInWindow() {}

InboundMessageResult InboundMessageValidator::validate(
    const NetworkEnvelope& envelope,
    const std::string& expectedNetworkId,
    const std::string& expectedChainId,
    const std::string& expectedProtocolVersion,
    std::int64_t now
) {
    if (!m_policy.isValid()) {
        return InboundMessageResult(InboundMessageStatus::INVALID_POLICY, "Inbound message policy is invalid.");
    }

    if (!envelope.isStructurallyValid(m_policy.maxPayloadBytes())) {
        return InboundMessageResult(InboundMessageStatus::INVALID_ENVELOPE, "Network envelope failed structural validation.");
    }

    if (envelope.networkId() != expectedNetworkId) {
        return InboundMessageResult(InboundMessageStatus::WRONG_NETWORK, "Network id mismatch.");
    }

    if (envelope.chainId() != expectedChainId) {
        return InboundMessageResult(InboundMessageStatus::WRONG_CHAIN, "Chain id mismatch.");
    }

    if (envelope.protocolVersion() != expectedProtocolVersion) {
        return InboundMessageResult(InboundMessageStatus::WRONG_PROTOCOL, "Protocol version mismatch.");
    }

    if (envelope.ttlSeconds() > m_policy.maxTtlSeconds()) {
        return InboundMessageResult(InboundMessageStatus::TTL_TOO_LARGE, "Message TTL exceeds local policy.");
    }

    if (envelope.expiredAt(now)) {
        return InboundMessageResult(InboundMessageStatus::EXPIRED, "Message already expired.");
    }

    const std::int64_t delta = now - envelope.createdAt();
    if (delta < 0 && std::llabs(delta) > m_policy.maxClockSkewSeconds()) {
        return InboundMessageResult(InboundMessageStatus::CLOCK_SKEW, "Message timestamp is too far in the future.");
    }

    pruneExpiredState(now);

    if (m_seenMessageIds.find(envelope.messageId()) != m_seenMessageIds.end()) {
        return InboundMessageResult(InboundMessageStatus::DUPLICATE_MESSAGE, "Duplicate message id.");
    }

    PeerWindowCounter& peerWindow =
        m_messagesByPeerInWindow[envelope.senderNodeId()];

    if (peerWindow.windowStartedAt <= 0 ||
        now < peerWindow.windowStartedAt ||
        now - peerWindow.windowStartedAt >
            static_cast<std::int64_t>(m_policy.maxTtlSeconds())) {
        peerWindow.windowStartedAt = now;
        peerWindow.count = 0;
    }

    if (peerWindow.count >= m_policy.maxMessagesPerPeerWindow()) {
        return InboundMessageResult(InboundMessageStatus::RATE_LIMITED, "Peer exceeded message window policy.");
    }

    const std::int64_t ttlSeconds =
        static_cast<std::int64_t>(envelope.ttlSeconds());
    const std::int64_t ttlWithSkew =
        m_policy.maxClockSkewSeconds() >
                std::numeric_limits<std::int64_t>::max() - ttlSeconds
            ? std::numeric_limits<std::int64_t>::max()
            : ttlSeconds + m_policy.maxClockSkewSeconds();
    const std::int64_t expiresAt =
        envelope.createdAt() >
                std::numeric_limits<std::int64_t>::max() - ttlWithSkew
            ? std::numeric_limits<std::int64_t>::max()
            : envelope.createdAt() + ttlWithSkew;

    m_seenMessageIds[envelope.messageId()] = expiresAt;
    ++peerWindow.count;

    return InboundMessageResult(InboundMessageStatus::ACCEPTED, "Inbound message accepted.");
}

std::size_t InboundMessageValidator::seenMessageCount() const {
    return m_seenMessageIds.size();
}

std::size_t InboundMessageValidator::peerWindowCount(const std::string& senderNodeId) const {
    const auto found = m_messagesByPeerInWindow.find(senderNodeId);
    return found == m_messagesByPeerInWindow.end() ? 0 : found->second.count;
}

void InboundMessageValidator::pruneExpiredState(std::int64_t now) {
    if (now <= 0) {
        return;
    }

    for (auto iterator = m_seenMessageIds.begin();
         iterator != m_seenMessageIds.end();) {
        if (iterator->second <= now) {
            iterator = m_seenMessageIds.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = m_messagesByPeerInWindow.begin();
         iterator != m_messagesByPeerInWindow.end();) {
        const PeerWindowCounter& window = iterator->second;
        if (window.windowStartedAt <= 0 ||
            now < window.windowStartedAt ||
            now - window.windowStartedAt >
                static_cast<std::int64_t>(m_policy.maxTtlSeconds())) {
            iterator = m_messagesByPeerInWindow.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

} // namespace nodo::p2p
