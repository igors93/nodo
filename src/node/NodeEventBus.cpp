#include "node/NodeEventBus.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {
namespace {

std::string jsonString(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

bool looksLikeJsonObject(const std::string &value) {
  return value.size() >= 2 && value.front() == '{' && value.back() == '}';
}

} // namespace

std::string nodeEventTypeToString(NodeEventType type) {
  switch (type) {
  case NodeEventType::NEW_BLOCK:
    return "new_block";
  case NodeEventType::BLOCK_FINALIZED:
    return "block_finalized";
  case NodeEventType::TX_ADMITTED:
    return "tx_admitted";
  case NodeEventType::TX_CONFIRMED:
    return "tx_confirmed";
  case NodeEventType::VALIDATOR_SLASHED:
    return "validator_slashed";
  case NodeEventType::PROPOSAL_CREATED:
    return "proposal_created";
  case NodeEventType::PROPOSAL_APPROVED:
    return "proposal_approved";
  case NodeEventType::TREASURY_EXECUTED:
    return "treasury_executed";
  case NodeEventType::PEER_CONNECTED:
    return "peer_connected";
  case NodeEventType::PEER_BANNED:
    return "peer_banned";
  case NodeEventType::LIGHT_CLIENT_CHECKPOINT:
    return "light_client_checkpoint";
  case NodeEventType::RPC_SUBSCRIPTION:
    return "rpc_subscription";
  }
  return "unknown";
}

std::optional<NodeEventType> nodeEventTypeFromString(const std::string &value) {
  if (value == "new_block")
    return NodeEventType::NEW_BLOCK;
  if (value == "block_finalized")
    return NodeEventType::BLOCK_FINALIZED;
  if (value == "tx_admitted")
    return NodeEventType::TX_ADMITTED;
  if (value == "tx_confirmed")
    return NodeEventType::TX_CONFIRMED;
  if (value == "validator_slashed")
    return NodeEventType::VALIDATOR_SLASHED;
  if (value == "proposal_created")
    return NodeEventType::PROPOSAL_CREATED;
  if (value == "proposal_approved")
    return NodeEventType::PROPOSAL_APPROVED;
  if (value == "treasury_executed")
    return NodeEventType::TREASURY_EXECUTED;
  if (value == "peer_connected")
    return NodeEventType::PEER_CONNECTED;
  if (value == "peer_banned")
    return NodeEventType::PEER_BANNED;
  if (value == "light_client_checkpoint")
    return NodeEventType::LIGHT_CLIENT_CHECKPOINT;
  if (value == "rpc_subscription")
    return NodeEventType::RPC_SUBSCRIPTION;
  return std::nullopt;
}

ChainEvent::ChainEvent()
    : m_sequence(0), m_type(NodeEventType::NEW_BLOCK), m_blockHeight(0),
      m_blockHash(), m_payloadJson("{}"), m_occurredAt(0) {}

ChainEvent::ChainEvent(std::uint64_t sequence, NodeEventType type,
                       std::uint64_t blockHeight, std::string blockHash,
                       std::string payloadJson, std::int64_t occurredAt)
    : m_sequence(sequence), m_type(type), m_blockHeight(blockHeight),
      m_blockHash(std::move(blockHash)),
      m_payloadJson(payloadJson.empty() ? "{}" : std::move(payloadJson)),
      m_occurredAt(occurredAt) {}

std::uint64_t ChainEvent::sequence() const { return m_sequence; }
NodeEventType ChainEvent::type() const { return m_type; }
std::uint64_t ChainEvent::blockHeight() const { return m_blockHeight; }
const std::string &ChainEvent::blockHash() const { return m_blockHash; }
const std::string &ChainEvent::payloadJson() const { return m_payloadJson; }
std::int64_t ChainEvent::occurredAt() const { return m_occurredAt; }

bool ChainEvent::isValid() const {
  return m_sequence > 0 && m_occurredAt > 0 &&
         looksLikeJsonObject(m_payloadJson);
}

std::string ChainEvent::serializeJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"sequence\":" << m_sequence
      << ",\"type\":" << jsonString(nodeEventTypeToString(m_type))
      << ",\"blockHeight\":" << m_blockHeight
      << ",\"blockHash\":" << jsonString(m_blockHash)
      << ",\"occurredAt\":" << m_occurredAt << ",\"payload\":"
      << (looksLikeJsonObject(m_payloadJson) ? m_payloadJson : "{}") << "}";
  return oss.str();
}

NodeEventBus::NodeEventBus(std::size_t maxRetainedEvents)
    : m_maxRetainedEvents(std::max<std::size_t>(maxRetainedEvents, 1)),
      m_nextSequence(1), m_events() {}

ChainEvent NodeEventBus::publish(NodeEventType type, std::uint64_t blockHeight,
                                 std::string blockHash, std::string payloadJson,
                                 std::int64_t occurredAt) {
  std::lock_guard<std::mutex> lock(m_mutex);
  ChainEvent event(m_nextSequence++, type, blockHeight, std::move(blockHash),
                   std::move(payloadJson), occurredAt);
  m_events.push_back(event);
  while (m_events.size() > m_maxRetainedEvents) {
    m_events.pop_front();
  }
  return event;
}

std::vector<ChainEvent>
NodeEventBus::recent(std::uint64_t afterSequence, std::size_t limit,
                     std::optional<NodeEventType> type) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (limit == 0) {
    return {};
  }
  limit = std::min<std::size_t>(limit, 1000);
  std::vector<ChainEvent> out;
  out.reserve(std::min(limit, m_events.size()));
  for (const ChainEvent &event : m_events) {
    if (event.sequence() <= afterSequence) {
      continue;
    }
    if (type.has_value() && event.type() != type.value()) {
      continue;
    }
    out.push_back(event);
    if (out.size() >= limit) {
      break;
    }
  }
  return out;
}

std::uint64_t NodeEventBus::latestSequence() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_nextSequence == 0 ? 0 : m_nextSequence - 1;
}

std::size_t NodeEventBus::retainedCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_events.size();
}

std::size_t NodeEventBus::maxRetainedEvents() const {
  return m_maxRetainedEvents;
}

std::string NodeEventBus::recentJson(std::uint64_t afterSequence,
                                     std::size_t limit,
                                     std::optional<NodeEventType> type) const {
  const std::vector<ChainEvent> events = recent(afterSequence, limit, type);
  std::ostringstream oss;
  oss << "{\"latestSequence\":" << latestSequence() << ",\"events\":[";
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << events[i].serializeJson();
  }
  oss << "],\"count\":" << events.size() << "}";
  return oss.str();
}

} // namespace nodo::node
