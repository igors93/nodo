#include "p2p/LoopbackTransport.hpp"

#include <utility>

namespace nodo::p2p {

LoopbackTransportBus::LoopbackTransportBus()
    : m_connections(),
      m_inboxByNodeId() {}

bool LoopbackTransportBus::isSafeNodeId(const std::string& nodeId) {
    if (nodeId.empty() || nodeId.size() > 160) {
        return false;
    }

    for (const char character : nodeId) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string LoopbackTransportBus::connectionKey(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    return localNodeId + "->" + remoteNodeId;
}

TransportResult LoopbackTransportBus::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    if (!isSafeNodeId(localNodeId) || !isSafeNodeId(remoteNodeId) || localNodeId == remoteNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Invalid loopback connection request."
        );
    }

    m_connections.insert(connectionKey(localNodeId, remoteNodeId));
    m_inboxByNodeId.emplace(localNodeId, std::deque<TransportMessage>{});
    m_inboxByNodeId.emplace(remoteNodeId, std::deque<TransportMessage>{});

    return TransportResult(
        TransportStatus::SENT,
        "Loopback connection registered."
    );
}

TransportResult LoopbackTransportBus::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    m_connections.erase(connectionKey(localNodeId, remoteNodeId));

    return TransportResult(
        TransportStatus::SENT,
        "Loopback connection removed."
    );
}

bool LoopbackTransportBus::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return m_connections.find(connectionKey(localNodeId, remoteNodeId)) != m_connections.end();
}

TransportResult LoopbackTransportBus::deliver(const TransportMessage& message) {
    if (!message.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "Transport message is invalid."
        );
    }

    if (!connected(message.fromNodeId(), message.toNodeId())) {
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "Loopback peers are not connected."
        );
    }

    m_inboxByNodeId[message.toNodeId()].push_back(message);

    return TransportResult(
        TransportStatus::SENT,
        "Loopback message delivered."
    );
}

std::optional<TransportMessage> LoopbackTransportBus::poll(
    const std::string& localNodeId
) {
    auto inbox = m_inboxByNodeId.find(localNodeId);

    if (inbox == m_inboxByNodeId.end() || inbox->second.empty()) {
        return std::nullopt;
    }

    TransportMessage message = inbox->second.front();
    inbox->second.pop_front();
    return message;
}

std::size_t LoopbackTransportBus::queuedMessageCount(const std::string& nodeId) const {
    const auto inbox = m_inboxByNodeId.find(nodeId);

    if (inbox == m_inboxByNodeId.end()) {
        return 0;
    }

    return inbox->second.size();
}

std::size_t LoopbackTransportBus::totalQueuedMessageCount() const {
    std::size_t total = 0;

    for (const auto& [nodeId, inbox] : m_inboxByNodeId) {
        (void)nodeId;
        total += inbox.size();
    }

    return total;
}

LoopbackTransport::LoopbackTransport()
    : m_bus(std::make_shared<LoopbackTransportBus>()) {}

LoopbackTransport::LoopbackTransport(std::shared_ptr<LoopbackTransportBus> bus)
    : m_bus(std::move(bus)) {
    if (!m_bus) {
        m_bus = std::make_shared<LoopbackTransportBus>();
    }
}

TransportResult LoopbackTransport::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    return m_bus->connect(localNodeId, remoteNodeId);
}

TransportResult LoopbackTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    return m_bus->disconnect(localNodeId, remoteNodeId);
}

bool LoopbackTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    return m_bus->connected(localNodeId, remoteNodeId);
}

TransportResult LoopbackTransport::send(const TransportMessage& message) {
    return m_bus->deliver(message);
}

std::optional<TransportMessage> LoopbackTransport::poll(const std::string& localNodeId) {
    return m_bus->poll(localNodeId);
}

std::shared_ptr<LoopbackTransportBus> LoopbackTransport::bus() const {
    return m_bus;
}

} // namespace nodo::p2p
