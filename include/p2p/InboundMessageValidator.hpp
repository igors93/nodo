#ifndef NODO_P2P_INBOUND_MESSAGE_VALIDATOR_HPP
#define NODO_P2P_INBOUND_MESSAGE_VALIDATOR_HPP

#include "p2p/NetworkEnvelope.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace nodo::p2p {

class InboundMessagePolicy {
public:
  InboundMessagePolicy();

  InboundMessagePolicy(std::size_t maxPayloadBytes, std::uint32_t maxTtlSeconds,
                       std::int64_t maxClockSkewSeconds,
                       std::size_t maxMessagesPerPeerWindow);

  InboundMessagePolicy(std::size_t maxPayloadBytes, std::uint32_t maxTtlSeconds,
                       std::int64_t maxClockSkewSeconds,
                       std::size_t maxMessagesPerPeerWindow,
                       std::uint32_t peerWindowSeconds);

  std::size_t
  maxPayloadBytes(NetworkMessageType type = NetworkMessageType::UNKNOWN) const;
  void setMaxPayloadBytes(NetworkMessageType type, std::size_t maxBytes);
  std::uint32_t maxTtlSeconds() const;
  std::int64_t maxClockSkewSeconds() const;
  std::size_t maxMessagesPerPeerWindow() const;
  std::uint32_t peerWindowSeconds() const;

  bool isValid() const;

private:
  std::size_t m_maxPayloadBytes;
  std::map<NetworkMessageType, std::size_t> m_maxPayloadBytesPerType;
  std::uint32_t m_maxTtlSeconds;
  std::int64_t m_maxClockSkewSeconds;
  std::size_t m_maxMessagesPerPeerWindow;
  std::uint32_t m_peerWindowSeconds;
};

enum class InboundMessageStatus {
  ACCEPTED,
  INVALID_POLICY,
  INVALID_ENVELOPE,
  WRONG_NETWORK,
  WRONG_CHAIN,
  WRONG_PROTOCOL,
  EXPIRED,
  TTL_TOO_LARGE,
  CLOCK_SKEW,
  DUPLICATE_MESSAGE,
  RATE_LIMITED
};

std::string inboundMessageStatusToString(InboundMessageStatus status);

class InboundMessageResult {
public:
  InboundMessageResult();
  InboundMessageResult(InboundMessageStatus status, std::string reason);

  InboundMessageStatus status() const;
  const std::string &reason() const;
  bool accepted() const;

private:
  InboundMessageStatus m_status;
  std::string m_reason;
};

class InboundMessageValidator {
public:
  explicit InboundMessageValidator(
      InboundMessagePolicy policy = InboundMessagePolicy());

  InboundMessageResult validate(const NetworkEnvelope &envelope,
                                const std::string &expectedNetworkId,
                                const std::string &expectedChainId,
                                const std::string &expectedProtocolVersion,
                                std::int64_t now);

  std::size_t seenMessageCount() const;
  std::size_t peerWindowCount(const std::string &senderNodeId) const;

private:
  struct PeerWindowCounter {
    std::int64_t windowStartedAt = 0;
    std::size_t count = 0;
  };

  InboundMessagePolicy m_policy;
  std::map<std::string, std::int64_t> m_seenMessageIds;
  std::map<std::string, PeerWindowCounter> m_messagesByPeerInWindow;
  std::int64_t m_lastPruneTime;

  void pruneExpiredState(std::int64_t now);
};

} // namespace nodo::p2p

#endif
