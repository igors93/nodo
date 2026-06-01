#ifndef NODO_P2P_ENCRYPTED_PEER_TRANSPORT_HPP
#define NODO_P2P_ENCRYPTED_PEER_TRANSPORT_HPP

#include "p2p/EncryptedPeerChannel.hpp"
#include "p2p/Transport.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nodo::p2p {

class EncryptedPeerTransport final : public Transport {
public:
    explicit EncryptedPeerTransport(
        Transport& underlyingTransport
    );

    bool establishSession(
        const std::string& localNodeId,
        const std::string& remoteNodeId,
        const std::string& sharedSecret,
        std::int64_t now
    );

    bool hasSession(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const;

    std::size_t sessionCount() const;

    std::size_t rejectedFrameCount() const;

    TransportResult connect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) override;

    TransportResult disconnect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) override;

    bool connected(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const override;

    TransportResult send(
        const TransportMessage& message
    ) override;

    std::optional<TransportMessage> poll(
        const std::string& localNodeId
    ) override;

private:
    Transport& m_underlyingTransport;
    std::map<std::string, EncryptedPeerSession> m_sessionsByDirection;
    std::size_t m_rejectedFrameCount;

    static std::string directionKey(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    );

    EncryptedPeerSession* sessionForDirection(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    );

    const EncryptedPeerSession* sessionForDirection(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const;
};

} // namespace nodo::p2p

#endif
