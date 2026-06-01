#include "p2p/OutboundMessageQueue.hpp"

#include <utility>

namespace nodo::p2p {

OutboundQueueResult::OutboundQueueResult()
    : m_status(OutboundQueueStatus::REJECTED),
      m_reason("Uninitialized outbound queue result.") {}

OutboundQueueResult::OutboundQueueResult(OutboundQueueStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

OutboundQueueStatus OutboundQueueResult::status() const { return m_status; }
const std::string& OutboundQueueResult::reason() const { return m_reason; }
bool OutboundQueueResult::enqueued() const { return m_status == OutboundQueueStatus::ENQUEUED; }

OutboundMessageQueue::OutboundMessageQueue(std::size_t maxMessagesPerPeer)
    : m_maxMessagesPerPeer(maxMessagesPerPeer),
      m_messagesByPeer() {}

OutboundQueueResult OutboundMessageQueue::enqueue(
    const std::string& peerNodeId,
    const NetworkEnvelope& envelope
) {
    if (peerNodeId.empty() || m_maxMessagesPerPeer == 0) {
        return OutboundQueueResult(OutboundQueueStatus::REJECTED, "Outbound queue input is invalid.");
    }

    auto& queue = m_messagesByPeer[peerNodeId];
    if (queue.size() >= m_maxMessagesPerPeer) {
        return OutboundQueueResult(OutboundQueueStatus::REJECTED, "Outbound queue for peer is full.");
    }

    queue.push_back(envelope);
    return OutboundQueueResult(OutboundQueueStatus::ENQUEUED, "Message queued for peer.");
}

std::optional<NetworkEnvelope> OutboundMessageQueue::dequeue(const std::string& peerNodeId) {
    auto found = m_messagesByPeer.find(peerNodeId);
    if (found == m_messagesByPeer.end() || found->second.empty()) {
        return std::nullopt;
    }

    NetworkEnvelope envelope = found->second.front();
    found->second.pop_front();

    if (found->second.empty()) {
        m_messagesByPeer.erase(found);
    }

    return envelope;
}

std::size_t OutboundMessageQueue::sizeForPeer(const std::string& peerNodeId) const {
    const auto found = m_messagesByPeer.find(peerNodeId);
    return found == m_messagesByPeer.end() ? 0 : found->second.size();
}

std::size_t OutboundMessageQueue::totalSize() const {
    std::size_t total = 0;
    for (const auto& [_, queue] : m_messagesByPeer) {
        total += queue.size();
    }
    return total;
}

bool OutboundMessageQueue::empty() const {
    return totalSize() == 0;
}

} // namespace nodo::p2p
