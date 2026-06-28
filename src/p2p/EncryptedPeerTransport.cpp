#include "p2p/EncryptedPeerTransport.hpp"

#include <utility>

namespace nodo::p2p {

EncryptedPeerTransport::EncryptedPeerTransport(Transport& underlyingTransport)
    : m_underlyingTransport(underlyingTransport),
      m_outboundSessions(),
      m_inboundSessions(),
      m_stagedOutboundSessions(),
      m_pendingConnections(),
      m_connectionTransport(
          dynamic_cast<AuthenticatedConnectionTransport*>(
              &underlyingTransport)),
      m_rejectedFrameCount(0) {}

bool EncryptedPeerTransport::establishSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId,
    const std::string& sharedSecret,
    std::int64_t now
) {
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex> lock(m_mutex);
    EncryptedPeerSession session(
        localNodeId, remoteNodeId, sharedSecret, now);
    if (!session.isValid()) return false;
    const std::string key = directionKey(localNodeId, remoteNodeId);
    if (m_connectionTransport != nullptr) {
        const auto pending = m_pendingConnections.find(key);
        if (pending == m_pendingConnections.end() ||
            !m_connectionTransport->authenticateConnection(
                pending->second, remoteNodeId)) {
            return false;
        }
        m_pendingConnections.erase(pending);
    }
    m_inboundSessions[key] = std::move(session);
    return true;
}

bool EncryptedPeerTransport::removeSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    TransportConnectionId pendingConnectionId = 0;
    const std::string key = directionKey(localNodeId, remoteNodeId);
    std::size_t removed = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        removed = m_outboundSessions.erase(key) +
            m_inboundSessions.erase(key) +
            m_stagedOutboundSessions.erase(key);
        const auto pending = m_pendingConnections.find(key);
        if (pending != m_pendingConnections.end()) {
            pendingConnectionId = pending->second;
            m_pendingConnections.erase(pending);
        }
    }
    const bool rejected = pendingConnectionId != 0 &&
        m_connectionTransport != nullptr &&
        m_connectionTransport->rejectConnection(pendingConnectionId);
    return removed > 0 || rejected;
}

bool EncryptedPeerTransport::rejectPendingConnection(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    TransportConnectionId connectionId = 0;
    const std::string key = directionKey(localNodeId, remoteNodeId);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto pending = m_pendingConnections.find(key);
        if (pending == m_pendingConnections.end()) return false;
        connectionId = pending->second;
        m_pendingConnections.erase(pending);
    }
    return m_connectionTransport != nullptr &&
           m_connectionTransport->rejectConnection(connectionId);
}

bool EncryptedPeerTransport::hasOutboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return outboundSession(localNodeId, remoteNodeId) != nullptr;
}

bool EncryptedPeerTransport::hasInboundSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return inboundSession(localNodeId, remoteNodeId) != nullptr;
}

bool EncryptedPeerTransport::hasSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string key = directionKey(localNodeId, remoteNodeId);
    return m_outboundSessions.find(key) != m_outboundSessions.end() &&
           m_inboundSessions.find(key) != m_inboundSessions.end();
}

std::size_t EncryptedPeerTransport::sessionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t authenticatedSessions = 0;
    for (const auto& [key, ignored] : m_outboundSessions) {
        (void)ignored;
        if (m_inboundSessions.find(key) != m_inboundSessions.end()) {
            ++authenticatedSessions;
        }
    }
    return authenticatedSessions;
}

std::size_t EncryptedPeerTransport::rejectedFrameCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rejectedFrameCount;
}

void EncryptedPeerTransport::clearSessions() {
    std::vector<TransportConnectionId> pendingConnectionIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pendingConnectionIds.reserve(m_pendingConnections.size());
        for (const auto& [key, connectionId] : m_pendingConnections) {
            (void)key;
            pendingConnectionIds.push_back(connectionId);
        }
        m_outboundSessions.clear();
        m_inboundSessions.clear();
        m_stagedOutboundSessions.clear();
        m_pendingConnections.clear();
    }
    if (m_connectionTransport != nullptr) {
        for (const TransportConnectionId connectionId : pendingConnectionIds) {
            (void)m_connectionTransport->rejectConnection(connectionId);
        }
    }
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

    EncryptedPeerChannelFrame frame;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        EncryptedPeerSession* session = outboundSession(
            message.fromNodeId(), message.toNodeId());
        if (session == nullptr) {
            return TransportResult(
                TransportStatus::REJECTED,
                "Authenticated outbound peer session is not active."
            );
        }
        frame = session->sealEnvelope(
            message.envelope(), message.sentAt());
    }
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
        if (m_connectionTransport != nullptr &&
            message->hasConnectionId() &&
            message->envelope().messageType() ==
                NetworkMessageType::PEER_HELLO) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingConnections[directionKey(
                message->toNodeId(), message->fromNodeId())] =
                message->connectionId();
        }
        return message;
    }

    bool rejectConnection = false;
    try {
        const EncryptedPeerChannelFrame frame =
            EncryptedPeerChannelCodec::decodeFrameFromString(
                message->envelope().payload());
        EncryptedPeerOpenResult opened;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            EncryptedPeerSession* session = inboundSession(
                message->toNodeId(), message->fromNodeId());
            if (session == nullptr) {
                ++m_rejectedFrameCount;
                rejectConnection = true;
            } else {
                opened = session->openFrame(frame);
            }
        }
        if (rejectConnection) {
            if (m_connectionTransport != nullptr &&
                message->hasConnectionId()) {
                (void)m_connectionTransport->rejectConnection(
                    message->connectionId());
            }
            return std::nullopt;
        }
        if (!opened.opened() ||
            opened.envelope()->messageType() !=
                message->envelope().messageType() ||
            opened.envelope()->senderNodeId() != message->fromNodeId()) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                ++m_rejectedFrameCount;
            }
            if (m_connectionTransport != nullptr &&
                message->hasConnectionId()) {
                (void)m_connectionTransport->rejectConnection(
                    message->connectionId());
            }
            return std::nullopt;
        }
        if (m_connectionTransport != nullptr &&
            message->hasConnectionId() &&
            !m_connectionTransport->isConnectionAuthenticated(
                message->connectionId(), message->fromNodeId()) &&
            !m_connectionTransport->authenticateConnection(
                message->connectionId(), message->fromNodeId())) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                ++m_rejectedFrameCount;
            }
            (void)m_connectionTransport->rejectConnection(
                message->connectionId());
            return std::nullopt;
        }
        return TransportMessage(
            message->fromNodeId(),
            message->toNodeId(),
            *opened.envelope(),
            message->sentAt(),
            message->connectionId()
        );
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_rejectedFrameCount;
        }
        if (m_connectionTransport != nullptr && message->hasConnectionId()) {
            (void)m_connectionTransport->rejectConnection(
                message->connectionId());
        }
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
