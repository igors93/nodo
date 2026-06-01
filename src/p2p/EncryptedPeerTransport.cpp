#include "p2p/EncryptedPeerTransport.hpp"

#include <utility>

namespace nodo::p2p {

EncryptedPeerTransport::EncryptedPeerTransport(
    Transport& underlyingTransport
) : m_underlyingTransport(underlyingTransport),
    m_sessionsByDirection(),
    m_rejectedFrameCount(0) {}

bool EncryptedPeerTransport::establishSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId,
    const std::string& sharedSecret,
    std::int64_t now
) {
    EncryptedPeerSession session(
        localNodeId,
        remoteNodeId,
        sharedSecret,
        now
    );

    if (!session.isValid()) {
        return false;
    }

    m_sessionsByDirection[directionKey(localNodeId, remoteNodeId)] =
        std::move(session);
    return true;
}

bool EncryptedPeerTransport::hasSession(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return sessionForDirection(localNodeId, remoteNodeId) != nullptr;
}

std::size_t EncryptedPeerTransport::sessionCount() const {
    return m_sessionsByDirection.size();
}

std::size_t EncryptedPeerTransport::rejectedFrameCount() const {
    return m_rejectedFrameCount;
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
    return m_underlyingTransport.disconnect(localNodeId, remoteNodeId);
}

bool EncryptedPeerTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return m_underlyingTransport.connected(localNodeId, remoteNodeId);
}

TransportResult EncryptedPeerTransport::send(
    const TransportMessage& message
) {
    if (!message.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "Encrypted transport rejected invalid plaintext transport message."
        );
    }

    EncryptedPeerSession* session =
        sessionForDirection(message.fromNodeId(), message.toNodeId());

    if (session == nullptr) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Encrypted transport has no established session for peer direction."
        );
    }

    EncryptedPeerChannelFrame frame =
        session->sealEnvelope(
            message.envelope(),
            message.sentAt()
        );

    if (!frame.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "Encrypted transport failed to seal plaintext envelope."
        );
    }

    NetworkEnvelope encryptedEnvelope(
        message.envelope().networkId(),
        message.envelope().chainId(),
        message.envelope().protocolVersion(),
        message.envelope().messageType(),
        message.fromNodeId(),
        message.envelope().createdAt(),
        message.envelope().ttlSeconds(),
        EncryptedPeerChannelCodec::encodeFrameToString(frame)
    );

    TransportMessage encryptedMessage(
        message.fromNodeId(),
        message.toNodeId(),
        encryptedEnvelope,
        message.sentAt()
    );

    return m_underlyingTransport.send(encryptedMessage);
}

std::optional<TransportMessage> EncryptedPeerTransport::poll(
    const std::string& localNodeId
) {
    const std::optional<TransportMessage> encryptedMessage =
        m_underlyingTransport.poll(localNodeId);

    if (!encryptedMessage.has_value()) {
        return std::nullopt;
    }

    EncryptedPeerSession* session =
        sessionForDirection(
            encryptedMessage->toNodeId(),
            encryptedMessage->fromNodeId()
        );

    if (session == nullptr) {
        ++m_rejectedFrameCount;
        return std::nullopt;
    }

    try {
        const EncryptedPeerChannelFrame frame =
            EncryptedPeerChannelCodec::decodeFrameFromString(
                encryptedMessage->envelope().payload()
            );

        EncryptedPeerOpenResult opened =
            session->openFrame(frame);

        if (!opened.opened()) {
            ++m_rejectedFrameCount;
            return std::nullopt;
        }

        return TransportMessage(
            encryptedMessage->fromNodeId(),
            encryptedMessage->toNodeId(),
            *opened.envelope(),
            encryptedMessage->sentAt()
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
    return localNodeId + "->" + remoteNodeId;
}

EncryptedPeerSession* EncryptedPeerTransport::sessionForDirection(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    const auto found =
        m_sessionsByDirection.find(directionKey(localNodeId, remoteNodeId));

    if (found == m_sessionsByDirection.end()) {
        return nullptr;
    }

    return &found->second;
}

const EncryptedPeerSession* EncryptedPeerTransport::sessionForDirection(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    const auto found =
        m_sessionsByDirection.find(directionKey(localNodeId, remoteNodeId));

    if (found == m_sessionsByDirection.end()) {
        return nullptr;
    }

    return &found->second;
}

} // namespace nodo::p2p
