#ifndef NODO_NODE_NODE_EVENT_BUS_HPP
#define NODO_NODE_NODE_EVENT_BUS_HPP

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

enum class NodeEventType {
  NEW_BLOCK,
  BLOCK_FINALIZED,
  TX_ADMITTED,
  TX_CONFIRMED,
  VALIDATOR_SLASHED,
  PROPOSAL_CREATED,
  PROPOSAL_APPROVED,
  TREASURY_EXECUTED,
  PEER_CONNECTED,
  PEER_BANNED,
  LIGHT_CLIENT_CHECKPOINT,
  RPC_SUBSCRIPTION
};

std::string nodeEventTypeToString(NodeEventType type);
std::optional<NodeEventType> nodeEventTypeFromString(const std::string &value);

class ChainEvent {
public:
  ChainEvent();
  ChainEvent(std::uint64_t sequence, NodeEventType type,
             std::uint64_t blockHeight, std::string blockHash,
             std::string payloadJson, std::int64_t occurredAt);

  std::uint64_t sequence() const;
  NodeEventType type() const;
  std::uint64_t blockHeight() const;
  const std::string &blockHash() const;
  const std::string &payloadJson() const;
  std::int64_t occurredAt() const;

  bool isValid() const;
  std::string serializeJson() const;

private:
  std::uint64_t m_sequence;
  NodeEventType m_type;
  std::uint64_t m_blockHeight;
  std::string m_blockHash;
  std::string m_payloadJson;
  std::int64_t m_occurredAt;
};

class NodeEventBus {
public:
  explicit NodeEventBus(std::size_t maxRetainedEvents = 4096);

  ChainEvent publish(NodeEventType type, std::uint64_t blockHeight,
                     std::string blockHash, std::string payloadJson,
                     std::int64_t occurredAt);

  std::vector<ChainEvent>
  recent(std::uint64_t afterSequence = 0, std::size_t limit = 100,
         std::optional<NodeEventType> type = std::nullopt) const;

  std::uint64_t latestSequence() const;
  std::size_t retainedCount() const;
  std::size_t maxRetainedEvents() const;

  std::string
  recentJson(std::uint64_t afterSequence = 0, std::size_t limit = 100,
             std::optional<NodeEventType> type = std::nullopt) const;

private:
  std::size_t m_maxRetainedEvents;
  mutable std::mutex m_mutex;
  std::uint64_t m_nextSequence;
  std::deque<ChainEvent> m_events;
};

} // namespace nodo::node

#endif
