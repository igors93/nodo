#ifndef NODO_P2P_LOOPBACK_TRANSPORT_HPP
#define NODO_P2P_LOOPBACK_TRANSPORT_HPP

#include "p2p/Transport.hpp"

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace nodo::p2p {

class LoopbackTransportBus {
public:
    LoopbackTransportBus();

    TransportResult connect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    );

    TransportResult disconnect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    );

    bool connected(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const;

    TransportResult deliver(
        const TransportMessage& message
    );

    std::optional<TransportMessage> poll(
        const std::string& localNodeId
    );

    std::size_t queuedMessageCount(
        const std::string& nodeId
    ) const;

    std::size_t totalQueuedMessageCount() const;

private:
    std::set<std::string> m_connections;
    std::map<std::string, std::deque<TransportMessage>> m_inboxByNodeId;

    static std::string connectionKey(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    );

    static bool isSafeNodeId(
        const std::string& nodeId
    );
};

class LoopbackTransport final : public Transport {
public:
    LoopbackTransport();

    explicit LoopbackTransport(
        std::shared_ptr<LoopbackTransportBus> bus
    );

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

    std::shared_ptr<LoopbackTransportBus> bus() const;

private:
    std::shared_ptr<LoopbackTransportBus> m_bus;
};

} // namespace nodo::p2p

#endif
