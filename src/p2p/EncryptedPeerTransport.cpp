#include "p2p/EncryptedPeerTransport.hpp"

#include <set>
#include <utility>

namespace nodo::p2p {

EncryptedPeerTransport::EncryptedPeerTransport(Transport& underlyingTransport)
    : m_underlyingTransport(underlyingTransport),
      m_outboundSessions(),
      m_inboundSessions(),
      m_stagedOutboundSessions(),
      m_rejectedFrameCount(0) {}

bool EncryptedPeerTransport::establishSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId,
    const std::string& sharedSecret,
    std::int64_t now
) {
    EncryptedPeerSession outbound(
        localNodeId, remoteNodeId, sharedSecret, now);
    EncryptedPeerSession inbound(
        localNodeId, remoteNodeId, sharedSecret, now);
    if (!outbound.isValid() || !inbound.isValid()) return false;

    const std::string key = directionKey(localNodeId, remoteNodeId);
    m_outboundSessions[key] = std::move(outbound);
    m_inboundSessions[key] = std::move(inbound);
    m_stagedOutboundSessions.erase(key);
    return true;
}

bool EncryptedPeerTransport::stageOutboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId,
    const std::string& sharedSecret,
    std::int64_t now
) {
    EncryptedPeerSession session(
        localNodeId, remoteNodeId, sharedSecret, now);
    if (!session.isValid()) return false;
    m_stagedOutboundSessions[directionKey(localNodeId, remoteNodeId)] =
        std::move(session);
    return true;
}

bool EncryptedPeerTransport::activateOutboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    const std::string key = directionKey(localNodeId, remoteNodeId);
    const auto found = m_stagedOutboundSessions.find(key);
    if (found == m_stagedOutboundSessions.end()) return false;
    m_outboundSessions[key] = std::move(found->second);
    m_stagedOutboundSessions.erase(found);
    return true;
}

bool EncryptedPeerTransport::establishInboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId,
    const std::string& sharedSecret,
    std::int64_t now
) {
    EncryptedPeerSession session(
        localNodeId, remoteNodeId, sharedSecret, now);
    if (!session.isValid()) return false;
    m_inboundSessions[directionKey(localNodeId, remoteNodeId)] =
        std::move(session);
    return true;
}

bool EncryptedPeerTransport::removeSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    const std::string key = directionKey(localNodeId, remoteNodeId);
    const std::size_t removed = m_outboundSessions.erase(key) +
        m_inboundSessions.erase(key) +
        m_stagedOutboundSessions.erase(key);
    return removed > 0;
}

bool EncryptedPeerTransport::hasOutboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return outboundSession(localNodeId, remoteNodeId) != nullptr;
}

bool EncryptedPeerTransport::hasInboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return inboundSession(localNodeId, remoteNodeId) != nullptr;
}

bool EncryptedPeerTransport::hasSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return hasOutboundSession(localNodeId, remoteNodeId) &&
           hasInboundSession(localNodeId, remoteNodeId);
}

std::size_t EncryptedPeerTransport::sessionCount() const {
    std::set<std::string> directions;
    for (const auto& [key, _] : m_outboundSessions) directions.insert(key);
    for (const auto& [key, _] : m_inboundSessions) directions.insert(key);
    return directions.size();
}

std::size_t EncryptedPeerTransport::rejectedFrameCount() const {
    return m_rejectedFrameCount;
}

void EncryptedPeerTransport::clearSessions() {
    m_outboundSessions.clear();
    m_inboundSessions.clear();
    m_stagedOutboundSessions.clear();
}

TransportResult EncryptedPeerTransport::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    return m_underlyingTransport.connect(localNodeId, remoteNodeId);
}

TransportResult EncryptedPeerTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    (void)removeSession(localNodeId, remoteNodeId);
    return m_underlyingTransport.disconnect(localNodeId, remoteNodeId);
}

bool EncryptedPeerTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return m_underlyingTransport.connected(localNodeId, remoteNodeId);
}

TransportResult EncryptedPeerTransport::send(const TransportMessage& message) {
    if (!message.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "Encrypted transport rejected invalid transport message."
        );
    }

    if (isHandshakeMessage(message.envelope().messageType())) {
        return m_underlyingTransport.send(message);
    }

    EncryptedPeerSession* session = outboundSession(
        message.fromNodeId(), message.toNodeId());
    if (session == nullptr) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Authenticated outbound peer session is not active."
        );
    }

    const EncryptedPeerChannelFrame frame = session->sealEnvelope(
        message.envelope(), message.sentAt());
    if (!frame.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "Encrypted transport failed to seal plaintext envelope."
        );
    }

    const NetworkEnvelope encryptedEnvelope(
        message.envelope().networkId(),
        message.envelope().chainId(),
        message.envelope().protocolVersion(),
        message.envelope().messageType(),
        message.fromNodeId(),
        message.envelope().createdAt(),
        message.envelope().ttlSeconds(),
        EncryptedPeerChannelCodec::encodeFrameToString(frame)
    );
    return m_underlyingTransport.send(TransportMessage(
        message.fromNodeId(),
        message.toNodeId(),
        encryptedEnvelope,
        message.sentAt()
    ));
}

std::optional<TransportMessage> EncryptedPeerTransport::poll(
    const std::string& localNodeId
) {
    const std::optional<TransportMessage> message =
        m_underlyingTransport.poll(localNodeId);
    if (!message.has_value()) return std::nullopt;

    if (isHandshakeMessage(message->envelope().messageType())) {
        return message;
    }

    EncryptedPeerSession* session = inboundSession(
        message->toNodeId(), message->fromNodeId());
    if (session == nullptr) {
        ++m_rejectedFrameCount;
        return std::nullopt;
    }

    try {
        const EncryptedPeerChannelFrame frame =
            EncryptedPeerChannelCodec::decodeFrameFromString(
                message->envelope().payload());
        const EncryptedPeerOpenResult opened = session->openFrame(frame);
        if (!opened.opened() ||
            opened.envelope()->messageType() !=
                message->envelope().messageType() ||
            opened.envelope()->senderNodeId() != message->fromNodeId()) {
            ++m_rejectedFrameCount;
            return std::nullopt;
        }
        return TransportMessage(
            message->fromNodeId(),
            message->toNodeId(),
            *opened.envelope(),
            message->sentAt()
        );
    } catch (...) {
        ++m_rejectedFrameCount;
        return std::nullopt;
    }
}

std::string EncryptedPeerTransport::directionKey(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    return std::to_string(localNodeId.size()) + ":" + localNodeId + "->" +
           std::to_string(remoteNodeId.size()) + ":" + remoteNodeId;
}

bool EncryptedPeerTransport::isHandshakeMessage(NetworkMessageType type) {
    return type == NetworkMessageType::PEER_CHALLENGE ||
           type == NetworkMessageType::PEER_HELLO;
}

EncryptedPeerSession* EncryptedPeerTransport::outboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    const auto found = m_outboundSessions.find(
        directionKey(localNodeId, remoteNodeId));
    return found == m_outboundSessions.end() ? nullptr : &found->second;
}

const EncryptedPeerSession* EncryptedPeerTransport::outboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    const auto found = m_outboundSessions.find(
        directionKey(localNodeId, remoteNodeId));
    return found == m_outboundSessions.end() ? nullptr : &found->second;
}

EncryptedPeerSession* EncryptedPeerTransport::inboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    const auto found = m_inboundSessions.find(
        directionKey(localNodeId, remoteNodeId));
    return found == m_inboundSessions.end() ? nullptr : &found->second;
}

const EncryptedPeerSession* EncryptedPeerTransport::inboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    const auto found = m_inboundSessions.find(
        directionKey(localNodeId, remoteNodeId));
    return found == m_inboundSessions.end() ? nullptr : &found->second;
}

} // namespace nodo::p2p
