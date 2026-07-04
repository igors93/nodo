#ifndef NODO_P2P_GOSSIP_MESH_HPP
#define NODO_P2P_GOSSIP_MESH_HPP

#include "node/EvidenceCaptureHealth.hpp"
#include "p2p/EclipseGuard.hpp"
#include "p2p/OutboundMessageQueue.hpp"
#include "p2p/PeerHandshakeReplayGuard.hpp"
#include "p2p/PeerRateLimiter.hpp"
#include "p2p/PeerRegistry.hpp"
#include "p2p/Transport.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nodo::storage {
class ProtocolEvidenceStore;
} // namespace nodo::storage

namespace nodo::p2p {

enum class PeerMisbehaviorType { INVALID_MESSAGE, RATE_LIMIT_EXCEEDED };

class GossipMeshConfig {
public:
  GossipMeshConfig();

  GossipMeshConfig(std::string localNodeId, std::string networkId,
                   std::string chainId, std::string protocolVersion,
                   std::string genesisId, std::uint32_t defaultTtlSeconds,
                   std::size_t invalidMessageQuarantineThreshold,
                   std::uint32_t maxGossipMessagesPerPeerWindow,
                   std::uint32_t maxTransactionGossipPerPeerWindow);

  GossipMeshConfig(std::string localNodeId, std::string networkId,
                   std::string chainId, std::string protocolVersion,
                   std::string genesisId, std::uint32_t defaultTtlSeconds,
                   std::size_t invalidMessageQuarantineThreshold,
                   bool requireAuthenticatedSessions, bool enforceEclipseGuard,
                   EclipseGuardConfig eclipseGuardConfig,
                   std::uint32_t maxGossipMessagesPerPeerWindow,
                   std::uint32_t maxTransactionGossipPerPeerWindow,
                   std::int64_t temporaryBanSeconds = 3600);

  const std::string &localNodeId() const;
  const std::string &networkId() const;
  const std::string &chainId() const;
  const std::string &protocolVersion() const;
  const std::string &genesisId() const;
  std::uint32_t defaultTtlSeconds() const;
  std::size_t invalidMessageQuarantineThreshold() const;
  std::int64_t temporaryBanSeconds() const;
  bool requireAuthenticatedSessions() const;
  bool enforceEclipseGuard() const;
  const EclipseGuardConfig &eclipseGuardConfig() const;
  std::uint32_t maxGossipMessagesPerPeerWindow() const;
  std::uint32_t maxTransactionGossipPerPeerWindow() const;

  bool isValid() const;

private:
  std::string m_localNodeId;
  std::string m_networkId;
  std::string m_chainId;
  std::string m_protocolVersion;
  std::string m_genesisId;
  std::uint32_t m_defaultTtlSeconds;
  std::size_t m_invalidMessageQuarantineThreshold;
  std::int64_t m_temporaryBanSeconds;
  bool m_requireAuthenticatedSessions;
  bool m_enforceEclipseGuard;
  EclipseGuardConfig m_eclipseGuardConfig;
  std::uint32_t m_maxGossipMessagesPerPeerWindow;
  std::uint32_t m_maxTransactionGossipPerPeerWindow;
};

class GossipDeliveryReport {
public:
  GossipDeliveryReport();
  GossipDeliveryReport(std::size_t acceptedCount, std::size_t rejectedCount);

  std::size_t acceptedCount() const;
  std::size_t rejectedCount() const;
  bool allAccepted() const;
  std::string serialize() const;

private:
  std::size_t m_acceptedCount;
  std::size_t m_rejectedCount;
};

class GossipInbox {
public:
  GossipInbox();

  void add(const NetworkEnvelope &envelope);

  std::size_t totalCount() const;
  std::size_t countForType(NetworkMessageType type) const;
  std::vector<NetworkEnvelope> messagesForType(NetworkMessageType type) const;

  // Removes and returns all messages of the given type.
  std::vector<NetworkEnvelope> drain(NetworkMessageType type);
  // Removes and returns all messages, grouped by type (flattened).
  std::vector<NetworkEnvelope> drainAll();

  std::string serialize() const;

private:
  std::map<NetworkMessageType, std::vector<NetworkEnvelope>> m_messagesByType;
};

class GossipMesh {
public:
  GossipMesh(GossipMeshConfig config, Transport &transport);

  // Optional overload: wire in an evidence store to persist protocol
  // violations.
  GossipMesh(GossipMeshConfig config, Transport &transport,
             storage::ProtocolEvidenceStore *evidenceStore);

  const GossipMeshConfig &config() const;
  Transport &transport();
  const Transport &transport() const;
  PeerRegistry &peerRegistry();
  const PeerRegistry &peerRegistry() const;
  PeerHandshakeReplayGuard &handshakeReplayGuard();
  const PeerHandshakeReplayGuard &handshakeReplayGuard() const;
  const GossipInbox &inbox() const;

  PeerRegistryResult registerPeer(PeerMetadata peer);

  TransportResult connectPeer(const std::string &remoteNodeId);

  TransportResult disconnectPeer(const std::string &remoteNodeId);

  NetworkEnvelope createEnvelope(NetworkMessageType type,
                                 const std::string &payload,
                                 std::int64_t now) const;

  GossipDeliveryReport broadcast(NetworkMessageType type,
                                 const std::string &payload, std::int64_t now);

  GossipDeliveryReport sendTo(const std::string &targetNodeId,
                              NetworkMessageType type,
                              const std::string &payload, std::int64_t now);

  // Injects a message directly into the local inbox (loopback for proposer).
  void injectLocalMessage(NetworkMessageType type, const std::string &payload,
                          std::int64_t now);

  // Only challenge and hello messages are allowed before authentication.
  GossipDeliveryReport sendHandshakeTo(const std::string &targetNodeId,
                                       NetworkMessageType type,
                                       const std::string &payload,
                                       std::int64_t now,
                                       TransportConnectionId connectionId = 0);

  TransportConnectionId
  takeConnectionIdForEnvelope(const NetworkEnvelope &envelope);

  GossipDeliveryReport flushOutbound(std::int64_t now);

  GossipDeliveryReport receiveAvailable(std::int64_t now);

  std::size_t invalidMessageCountForPeer(const std::string &nodeId) const;

  std::uint32_t rateLimitedMessageCountForPeer(const std::string &nodeId,
                                               std::int64_t now) const;

  std::size_t quarantineEscalationForPeer(const std::string &nodeId) const;

  void reportPeerMisbehavior(const NetworkEnvelope &envelope,
                             PeerMisbehaviorType type,
                             const std::string &reason, std::int64_t now);

  bool restorePeerPenaltyState(const std::string &nodeId,
                               std::size_t invalidMessageCount,
                               std::size_t escalationCount,
                               std::int32_t score = 100);

  bool restorePeerPenaltyState(const std::string &nodeId,
                               std::size_t invalidMessageCount,
                               std::size_t escalationCount,
                               std::int64_t bannedUntil,
                               const std::string &banReason, std::int64_t now,
                               std::int32_t score = 100);

  std::size_t liftExpiredPeerPenalties(std::int64_t now);

  bool peerBannedAt(const std::string &nodeId, std::int64_t now) const;

  bool isIpQuarantined(const std::string &ip) const;

  void setPeerPenaltyPersistenceHandler(std::function<void()> handler);

  bool peerPenaltyPersistenceHealthy() const;
  const std::string &lastPeerPenaltyPersistenceError() const;

  // Drains (removes and returns) all messages of the given type from the inbox.
  // Use instead of inbox().messagesForType() to avoid replay on each tick.
  std::vector<NetworkEnvelope> drainInbox(NetworkMessageType type);

  // Drains all messages from the inbox.
  std::vector<NetworkEnvelope> drainAllInbox();

  // Returns a snapshot of the current evidence capture health.
  // Use this to surface persistence failures through operator diagnostics.
  const node::EvidenceCaptureHealth &evidenceCaptureHealth() const;

private:
  GossipMeshConfig m_config;
  Transport &m_transport;
  storage::ProtocolEvidenceStore *m_evidenceStore;
  PeerRegistry m_peerRegistry;
  PeerHandshakeReplayGuard m_handshakeReplayGuard;
  OutboundMessageQueue m_outboundQueue;
  PeerRateLimiter m_rateLimiter;
  EclipseGuard m_eclipseGuard;
  GossipInbox m_inbox;
  std::map<std::string, TransportConnectionId> m_connectionByMessageId;
  std::map<std::string, std::size_t> m_invalidMessagesByIdentity;
  std::map<std::string, std::size_t> m_quarantineEscalationByIdentity;
  // Coalescing: tracks (identityKey, ruleId) -> last evidence timestamp.
  std::map<std::pair<std::string, std::string>, std::int64_t> m_lastEvidenceAt;
  node::EvidenceCaptureHealth m_evidenceCaptureHealth;
  std::function<void()> m_peerPenaltyPersistenceHandler;
  std::string m_lastPeerPenaltyPersistenceError;

  // LRU for m_invalidMessagesByIdentity
  std::list<std::string> m_lruInvalidMessages;
  std::map<std::string, std::list<std::string>::iterator>
      m_lruInvalidMessagesMap;
  static constexpr std::size_t MAX_INVALID_TRACKING = 4096;

  bool hasAuthenticatedInboundSession(const std::string &nodeId) const;

  bool hasAuthenticatedOutboundSession(const std::string &nodeId) const;

  bool requiresAuthenticatedEnvelope(NetworkMessageType type) const;

  std::vector<PeerSubnetInfo> activePeerSubnets() const;

  static std::optional<PeerSubnetInfo>
  subnetInfoForPeer(const PeerMetadata &peer);

  bool shouldQuarantinePeer(const std::string &nodeId) const;

  std::string identityKeyForNodeId(const std::string &nodeId) const;

  void recordInvalidMessage(
      const std::string &nodeId, const std::string &reason, std::int64_t now,
      PeerMisbehaviorType type = PeerMisbehaviorType::INVALID_MESSAGE,
      const NetworkEnvelope *envelope = nullptr);

  void persistPeerPenaltyState();
};

} // namespace nodo::p2p

#endif
