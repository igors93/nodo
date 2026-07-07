#include "p2p/OutboundMessageQueue.hpp"

#include <algorithm>
#include <utility>

namespace nodo::p2p {

OutboundQueueResult::OutboundQueueResult()
    : m_status(OutboundQueueStatus::REJECTED),
      m_reason("Uninitialized outbound queue result.") {}

OutboundQueueResult::OutboundQueueResult(OutboundQueueStatus status,
                                         std::string reason)
    : m_status(status), m_reason(std::move(reason)) {}

OutboundQueueStatus OutboundQueueResult::status() const { return m_status; }
const std::string &OutboundQueueResult::reason() const { return m_reason; }
bool OutboundQueueResult::enqueued() const {
  return m_status == OutboundQueueStatus::ENQUEUED;
}

OutboundMessageQueue::OutboundMessageQueue(std::size_t maxMessagesPerPeer)
    : m_maxMessagesPerPeer(maxMessagesPerPeer), m_messagesByPeer() {}

bool OutboundMessageQueue::isPriorityType(NetworkMessageType type) {
  switch (type) {
  case NetworkMessageType::PEER_CHALLENGE:
  case NetworkMessageType::PEER_HELLO:
  case NetworkMessageType::PEER_STATUS:
  case NetworkMessageType::CHAIN_STATUS:
  case NetworkMessageType::BLOCK_PROPOSAL:
  case NetworkMessageType::VALIDATOR_VOTE:
  case NetworkMessageType::QUORUM_CERTIFICATE:
  case NetworkMessageType::FINALIZED_BLOCK_ARTIFACT:
  case NetworkMessageType::BLOCK_SYNC_REQUEST:
  case NetworkMessageType::BLOCK_SYNC_RESPONSE:
  case NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE:
  case NetworkMessageType::SLASHING_EVIDENCE_INVENTORY:
  case NetworkMessageType::SLASHING_EVIDENCE_REQUEST:
  case NetworkMessageType::SLASHING_EVIDENCE_RESPONSE:
    return true;
  default:
    return false;
  }
}

bool OutboundMessageQueue::isPriorityEnvelope(
    const NetworkEnvelope &envelope) {
  return isPriorityType(envelope.messageType());
}

OutboundQueueResult OutboundMessageQueue::enqueue(
    const std::string &peerNodeId, const NetworkEnvelope &envelope) {
  if (peerNodeId.empty() || m_maxMessagesPerPeer == 0) {
    return OutboundQueueResult(OutboundQueueStatus::REJECTED,
                               "Outbound queue input is invalid.");
  }

  auto &queue = m_messagesByPeer[peerNodeId];
  const bool priority = isPriorityEnvelope(envelope);

  if (queue.size() >= m_maxMessagesPerPeer) {
    if (!priority) {
      return OutboundQueueResult(OutboundQueueStatus::REJECTED,
                                 "Outbound queue for peer is full.");
    }

    // Consensus, finality and sync messages must not be starved behind
    // best-effort gossip. If the peer queue is full, allow a priority message
    // to replace the newest low-priority entry. Never evict another priority
    // message, because that could drop a vote, finality proof or sync answer.
    auto removable = std::find_if(
        queue.rbegin(), queue.rend(), [](const NetworkEnvelope &queued) {
          return !isPriorityEnvelope(queued);
        });

    if (removable == queue.rend()) {
      return OutboundQueueResult(
          OutboundQueueStatus::REJECTED,
          "Outbound queue for peer is full of priority messages.");
    }

    queue.erase(std::next(removable).base());
  }

  if (priority) {
    // Preserve FIFO order among priority messages while placing them before
    // low-priority transaction gossip and other best-effort traffic.
    const auto firstLowPriority = std::find_if(
        queue.begin(), queue.end(), [](const NetworkEnvelope &queued) {
          return !isPriorityEnvelope(queued);
        });
    queue.insert(firstLowPriority, envelope);
  } else {
    queue.push_back(envelope);
  }

  return OutboundQueueResult(OutboundQueueStatus::ENQUEUED,
                             priority ? "Priority message queued for peer."
                                      : "Message queued for peer.");
}

std::optional<NetworkEnvelope>
OutboundMessageQueue::dequeue(const std::string &peerNodeId) {
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

std::size_t
OutboundMessageQueue::sizeForPeer(const std::string &peerNodeId) const {
  const auto found = m_messagesByPeer.find(peerNodeId);
  return found == m_messagesByPeer.end() ? 0 : found->second.size();
}

std::size_t OutboundMessageQueue::totalSize() const {
  std::size_t total = 0;
  for (const auto &[peer, messages] : m_messagesByPeer) {
    (void)peer;
    total += messages.size();
  }
  return total;
}

bool OutboundMessageQueue::empty() const { return totalSize() == 0; }

} // namespace nodo::p2p
