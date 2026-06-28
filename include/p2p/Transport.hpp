#ifndef NODO_P2P_TRANSPORT_HPP
#define NODO_P2P_TRANSPORT_HPP

#include "p2p/NetworkEnvelope.hpp"
#include "p2p/AuthenticatedConnectionTransport.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nodo::p2p {

enum class TransportStatus {
    SENT,
    REJECTED,
    NOT_CONNECTED,
    INVALID_MESSAGE
};

std::string transportStatusToString(TransportStatus status);

class TransportResult {
public:
    TransportResult();
    TransportResult(TransportStatus status, std::string reason);

    TransportStatus status() const;
    const std::string& reason() const;
    bool sent() const;
    bool success() const;
    std::string serialize() const;

private:
    TransportStatus m_status;
    std::string m_reason;
};

class TransportMessage {
public:
    TransportMessage();

    TransportMessage(
        std::string fromNodeId,
        std::string toNodeId,
        NetworkEnvelope envelope,
        std::int64_t sentAt
    );

    TransportMessage(
        std::string fromNodeId,
        std::string toNodeId,
        NetworkEnvelope envelope,
        std::int64_t sentAt,
        TransportConnectionId connectionId
    );

    const std::string& fromNodeId() const;
    const std::string& toNodeId() const;
    const NetworkEnvelope& envelope() const;
    std::int64_t sentAt() const;
    TransportConnectionId connectionId() const;
    bool hasConnectionId() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_fromNodeId;
    std::string m_toNodeId;
    NetworkEnvelope m_envelope;
    std::int64_t m_sentAt;
    TransportConnectionId m_connectionId;
};

class Transport {
public:
    virtual ~Transport() = default;

    virtual TransportResult connect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) = 0;

    virtual TransportResult disconnect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) = 0;

    virtual bool connected(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const = 0;

    virtual TransportResult send(
        const TransportMessage& message
    ) = 0;

    virtual std::optional<TransportMessage> poll(
        const std::string& localNodeId
    ) = 0;
};

} // namespace nodo::p2p

#endif
