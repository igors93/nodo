#ifndef NODO_P2P_OUTBOUND_MESSAGE_QUEUE_HPP
#define NODO_P2P_OUTBOUND_MESSAGE_QUEUE_HPP

#include "p2p/NetworkEnvelope.hpp"

#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>

namespace nodo::p2p {

enum class OutboundQueueStatus {
    ENQUEUED,
    REJECTED
};

class OutboundQueueResult {
public:
    OutboundQueueResult();
    OutboundQueueResult(OutboundQueueStatus status, std::string reason);

    OutboundQueueStatus status() const;
    const std::string& reason() const;
    bool enqueued() const;

private:
    OutboundQueueStatus m_status;
    std::string m_reason;
};

class OutboundMessageQueue {
public:
    explicit OutboundMessageQueue(std::size_t maxMessagesPerPeer = 1024);

    OutboundQueueResult enqueue(
        const std::string& peerNodeId,
        const NetworkEnvelope& envelope
    );

    std::optional<NetworkEnvelope> dequeue(
        const std::string& peerNodeId
    );

    std::size_t sizeForPeer(const std::string& peerNodeId) const;
    std::size_t totalSize() const;
    bool empty() const;

private:
    std::size_t m_maxMessagesPerPeer;
    std::map<std::string, std::deque<NetworkEnvelope>> m_messagesByPeer;
};

} // namespace nodo::p2p

#endif
