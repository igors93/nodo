#include "p2p/GossipMesh.hpp"

#include "core/ProtocolLimits.hpp"
#include "p2p/AuthenticatedSessionTransport.hpp"

#include "crypto/hash.h"
#include "economics/ProtocolEvidence.hpp"
#include "storage/ProtocolEvidenceStore.hpp"

#include <stdexcept>

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

constexpr std::int32_t kInvalidMessageScorePenalty = -10;
constexpr std::int32_t kRateLimitScorePenalty = -5;
constexpr std::int32_t kTemporaryBanScoreThreshold = -50;

bool isSafeScalar(const std::string &value) {
  if (value.empty() || value.size() > 200) {
    return false;
  }

  for (const char character : value) {
    const bool allowed = (character >= 'a' && character <= 'z') ||
                         (character >= 'A' && character <= 'Z') ||
                         (character >= '0' && character <= '9') ||
                         character == '_' || character == '-' ||
                         character == '.' || character == ':' ||
                         character == '/';

    if (!allowed) {
      return false;
    }
  }

  return true;
}

} // namespace

GossipMeshConfig::GossipMeshConfig()
    : m_localNodeId(""), m_networkId(""), m_chainId(""), m_protocolVersion(""),
      m_genesisId(""), m_defaultTtlSeconds(0),
      m_invalidMessageQuarantineThreshold(0), m_temporaryBanSeconds(3600),
      m_requireAuthenticatedSessions(false), m_enforceEclipseGuard(false),
      m_eclipseGuardConfig(EclipseGuardConfig::defaults()) {}

GossipMeshConfig::GossipMeshConfig(
    std::string localNodeId, std::string networkId, std::string chainId,
    std::string protocolVersion, std::string genesisId,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold)
    : m_localNodeId(std::move(localNodeId)), m_networkId(std::move(networkId)),
      m_chainId(std::move(chainId)),
      m_protocolVersion(std::move(protocolVersion)),
      m_genesisId(std::move(genesisId)), m_defaultTtlSeconds(defaultTtlSeconds),
      m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold),
      m_temporaryBanSeconds(3600), m_requireAuthenticatedSessions(false),
      m_enforceEclipseGuard(false),
      m_eclipseGuardConfig(EclipseGuardConfig::defaults()) {}

GossipMeshConfig::GossipMeshConfig(
    std::string localNodeId, std::string networkId, std::string chainId,
    std::string protocolVersion, std::string genesisId,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold,
    bool requireAuthenticatedSessions, bool enforceEclipseGuard,
    EclipseGuardConfig eclipseGuardConfig, std::int64_t temporaryBanSeconds)
    : m_localNodeId(std::move(localNodeId)), m_networkId(std::move(networkId)),
      m_chainId(std::move(chainId)),
      m_protocolVersion(std::move(protocolVersion)),
      m_genesisId(std::move(genesisId)), m_defaultTtlSeconds(defaultTtlSeconds),
      m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold),
      m_temporaryBanSeconds(temporaryBanSeconds),
      m_requireAuthenticatedSessions(requireAuthenticatedSessions),
      m_enforceEclipseGuard(enforceEclipseGuard),
      m_eclipseGuardConfig(std::move(eclipseGuardConfig)) {}

const std::string &GossipMeshConfig::localNodeId() const {
  return m_localNodeId;
}
const std::string &GossipMeshConfig::networkId() const { return m_networkId; }
const std::string &GossipMeshConfig::chainId() const { return m_chainId; }
const std::string &GossipMeshConfig::protocolVersion() const {
  return m_protocolVersion;
}
const std::string &GossipMeshConfig::genesisId() const { return m_genesisId; }
std::uint32_t GossipMeshConfig::defaultTtlSeconds() const {
  return m_defaultTtlSeconds;
}
std::size_t GossipMeshConfig::invalidMessageQuarantineThreshold() const {
  return m_invalidMessageQuarantineThreshold;
}
std::int64_t GossipMeshConfig::temporaryBanSeconds() const {
  return m_temporaryBanSeconds;
}
bool GossipMeshConfig::requireAuthenticatedSessions() const {
  return m_requireAuthenticatedSessions;
}
bool GossipMeshConfig::enforceEclipseGuard() const {
  return m_enforceEclipseGuard;
}
const EclipseGuardConfig &GossipMeshConfig::eclipseGuardConfig() const {
  return m_eclipseGuardConfig;
}

bool GossipMeshConfig::isValid() const {
  return isSafeScalar(m_localNodeId) && isSafeScalar(m_networkId) &&
         isSafeScalar(m_chainId) && isSafeScalar(m_protocolVersion) &&
         !m_genesisId.empty() && m_defaultTtlSeconds > 0 &&
         m_defaultTtlSeconds <= 3600 &&
         m_invalidMessageQuarantineThreshold > 0 && m_temporaryBanSeconds > 0 &&
         m_temporaryBanSeconds <= 86400 * 30 &&
         (!m_enforceEclipseGuard || m_eclipseGuardConfig.isValid());
}

GossipDeliveryReport::GossipDeliveryReport()
    : m_acceptedCount(0), m_rejectedCount(0) {}

GossipDeliveryReport::GossipDeliveryReport(std::size_t acceptedCount,
                                           std::size_t rejectedCount)
    : m_acceptedCount(acceptedCount), m_rejectedCount(rejectedCount) {}

std::size_t GossipDeliveryReport::acceptedCount() const {
  return m_acceptedCount;
}
std::size_t GossipDeliveryReport::rejectedCount() const {
  return m_rejectedCount;
}
bool GossipDeliveryReport::allAccepted() const { return m_rejectedCount == 0; }

std::string GossipDeliveryReport::serialize() const {
  std::ostringstream output;
  output << "GossipDeliveryReport{acceptedCount=" << m_acceptedCount
         << ";rejectedCount=" << m_rejectedCount << "}";
  return output.str();
}

GossipInbox::GossipInbox() : m_messagesByType() {}

void GossipInbox::add(const NetworkEnvelope &envelope) {
  m_messagesByType[envelope.messageType()].push_back(envelope);
}

std::size_t GossipInbox::totalCount() const {
  std::size_t total = 0;

  for (const auto &[type, messages] : m_messagesByType) {
    (void)type;
    total += messages.size();
  }

  return total;
}

std::size_t GossipInbox::countForType(NetworkMessageType type) const {
  const auto found = m_messagesByType.find(type);

  if (found == m_messagesByType.end()) {
    return 0;
  }

  return found->second.size();
}

std::vector<NetworkEnvelope>
GossipInbox::messagesForType(NetworkMessageType type) const {
  const auto found = m_messagesByType.find(type);

  if (found == m_messagesByType.end()) {
    return {};
  }

  return found->second;
}

std::vector<NetworkEnvelope> GossipInbox::drain(NetworkMessageType type) {
  const auto found = m_messagesByType.find(type);

  if (found == m_messagesByType.end()) {
    return {};
  }

  std::vector<NetworkEnvelope> messages = std::move(found->second);
  m_messagesByType.erase(found);
  return messages;
}

std::vector<NetworkEnvelope> GossipInbox::drainAll() {
  std::vector<NetworkEnvelope> all;

  for (auto &[type, messages] : m_messagesByType) {
    (void)type;
    all.insert(all.end(), std::make_move_iterator(messages.begin()),
               std::make_move_iterator(messages.end()));
  }

  m_messagesByType.clear();
  return all;
}

std::string GossipInbox::serialize() const {
  std::ostringstream output;
  output << "GossipInbox{totalCount=" << totalCount() << ";types=[";

  bool first = true;
  for (const auto &[type, messages] : m_messagesByType) {
    if (!first) {
      output << ",";
    }
    output << networkMessageTypeToString(type) << ":" << messages.size();
    first = false;
  }

  output << "]}";
  return output.str();
}

GossipMesh::GossipMesh(GossipMeshConfig config, Transport &transport)
    : m_config(std::move(config)), m_transport(transport),
      m_evidenceStore(nullptr), m_peerRegistry(), m_handshakeReplayGuard(),
      m_outboundQueue(1024), m_rateLimiter(),
      m_eclipseGuard(m_config.eclipseGuardConfig()), m_inbox(),
      m_connectionByMessageId(), m_invalidMessagesByIdentity(),
      m_lastEvidenceAt(), m_evidenceCaptureHealth(),
      m_peerPenaltyPersistenceHandler(), m_lastPeerPenaltyPersistenceError() {
  m_evidenceCaptureHealth.markUnavailable();
}

GossipMesh::GossipMesh(GossipMeshConfig config, Transport &transport,
                       storage::ProtocolEvidenceStore *evidenceStore)
    : m_config(std::move(config)), m_transport(transport),
      m_evidenceStore(evidenceStore), m_peerRegistry(),
      m_handshakeReplayGuard(), m_outboundQueue(1024), m_rateLimiter(),
      m_eclipseGuard(m_config.eclipseGuardConfig()), m_inbox(),
      m_connectionByMessageId(), m_invalidMessagesByIdentity(),
      m_lastEvidenceAt(), m_evidenceCaptureHealth(),
      m_peerPenaltyPersistenceHandler(), m_lastPeerPenaltyPersistenceError() {
  if (evidenceStore == nullptr) {
    m_evidenceCaptureHealth.markUnavailable();
  }
}

const GossipMeshConfig &GossipMesh::config() const { return m_config; }
Transport &GossipMesh::transport() { return m_transport; }
const Transport &GossipMesh::transport() const { return m_transport; }
PeerRegistry &GossipMesh::peerRegistry() { return m_peerRegistry; }
const PeerRegistry &GossipMesh::peerRegistry() const { return m_peerRegistry; }
PeerHandshakeReplayGuard &GossipMesh::handshakeReplayGuard() {
  return m_handshakeReplayGuard;
}
const PeerHandshakeReplayGuard &GossipMesh::handshakeReplayGuard() const {
  return m_handshakeReplayGuard;
}
const GossipInbox &GossipMesh::inbox() const { return m_inbox; }

std::vector<NetworkEnvelope> GossipMesh::drainInbox(NetworkMessageType type) {
  return m_inbox.drain(type);
}

std::vector<NetworkEnvelope> GossipMesh::drainAllInbox() {
  return m_inbox.drainAll();
}

const node::EvidenceCaptureHealth &GossipMesh::evidenceCaptureHealth() const {
  return m_evidenceCaptureHealth;
}

bool GossipMesh::requiresAuthenticatedEnvelope(NetworkMessageType type) const {
  return m_config.requireAuthenticatedSessions() &&
         type != NetworkMessageType::PEER_CHALLENGE &&
         type != NetworkMessageType::PEER_HELLO;
}

bool GossipMesh::hasAuthenticatedInboundSession(
    const std::string &nodeId) const {
  if (!m_config.requireAuthenticatedSessions()) {
    return true;
  }
  const auto *authTransport =
      dynamic_cast<const AuthenticatedSessionTransport *>(&m_transport);
  return authTransport != nullptr &&
         authTransport->hasInboundSession(m_config.localNodeId(), nodeId);
}

bool GossipMesh::hasAuthenticatedOutboundSession(
    const std::string &nodeId) const {
  if (!m_config.requireAuthenticatedSessions()) {
    return true;
  }
  const auto *authTransport =
      dynamic_cast<const AuthenticatedSessionTransport *>(&m_transport);
  return authTransport != nullptr &&
         authTransport->hasOutboundSession(m_config.localNodeId(), nodeId);
}

std::optional<PeerSubnetInfo>
GossipMesh::subnetInfoForPeer(const PeerMetadata &peer) {
  const std::string subnet =
      PeerSubnetInfo::extractSubnetPrefix(peer.endpoint().host());
  if (subnet.empty()) {
    return std::nullopt;
  }
  PeerSubnetInfo info;
  info.peerId = peer.nodeId();
  info.ipAddress = peer.endpoint().host();
  info.subnetPrefix = subnet;
  info.port = peer.endpoint().port();
  return info.isValid() ? std::optional<PeerSubnetInfo>(info) : std::nullopt;
}

std::vector<PeerSubnetInfo> GossipMesh::activePeerSubnets() const {
  std::vector<PeerSubnetInfo> peers;
  for (const PeerMetadata &peer : m_peerRegistry.activePeers()) {
    const auto info = subnetInfoForPeer(peer);
    if (info.has_value()) {
      peers.push_back(*info);
    }
  }
  return peers;
}

PeerRegistryResult GossipMesh::registerPeer(PeerMetadata peer) {
  if (!m_config.isValid()) {
    return PeerRegistryResult(PeerRegistryStatus::REJECTED,
                              "Gossip mesh config is invalid.");
  }

  if (peer.nodeId() == m_config.localNodeId()) {
    return PeerRegistryResult(PeerRegistryStatus::REJECTED,
                              "Cannot register local node as remote peer.");
  }

  if (m_config.enforceEclipseGuard()) {
    const PeerMetadata *existing = m_peerRegistry.peer(peer.nodeId());
    bool skipEclipseCheck = false;

    if (existing != nullptr) {
      auto oldInfo = subnetInfoForPeer(*existing);
      auto newInfo = subnetInfoForPeer(peer);
      if (oldInfo.has_value() && newInfo.has_value() &&
          oldInfo->subnetPrefix == newInfo->subnetPrefix) {
        skipEclipseCheck = true;
      }
    }

    if (!skipEclipseCheck) {
      const std::optional<PeerSubnetInfo> candidate = subnetInfoForPeer(peer);
      if (!candidate.has_value()) {
        return PeerRegistryResult(
            PeerRegistryStatus::REJECTED,
            "Peer endpoint cannot be classified for eclipse protection.");
      }

      std::vector<PeerSubnetInfo> currentSubnets = activePeerSubnets();
      if (existing != nullptr) {
        auto oldInfo = subnetInfoForPeer(*existing);
        if (oldInfo.has_value()) {
          auto it = std::find_if(currentSubnets.begin(), currentSubnets.end(),
                                 [&](const PeerSubnetInfo &info) {
                                   return info.peerId == oldInfo->peerId;
                                 });
          if (it != currentSubnets.end()) {
            currentSubnets.erase(it);
          }
        }
      }

      const EclipseCheckResult admission =
          m_eclipseGuard.checkAdmission(*candidate, currentSubnets);
      if (!admission.isAllowed()) {
        return PeerRegistryResult(PeerRegistryStatus::REJECTED,
                                  "Eclipse guard rejected peer admission: " +
                                      admission.detail());
      }
    }
  }

  return m_peerRegistry.registerPeer(std::move(peer));
}

TransportResult GossipMesh::connectPeer(const std::string &remoteNodeId) {
  const PeerMetadata *peer = m_peerRegistry.peer(remoteNodeId);
  if (peer == nullptr) {
    return TransportResult(TransportStatus::REJECTED,
                           "Cannot connect unknown peer.");
  }

  if (peer->quarantined() || peer->bannedUntil() > 0) {
    return TransportResult(
        TransportStatus::REJECTED,
        "Cannot connect quarantined or temporarily banned peer.");
  }

  return m_transport.connect(m_config.localNodeId(), remoteNodeId);
}

TransportResult GossipMesh::disconnectPeer(const std::string &remoteNodeId) {
  if (remoteNodeId == m_config.localNodeId()) {
    return TransportResult(TransportStatus::REJECTED,
                           "Cannot disconnect self.");
  }
  return m_transport.disconnect(m_config.localNodeId(), remoteNodeId);
}

NetworkEnvelope GossipMesh::createEnvelope(NetworkMessageType type,
                                           const std::string &payload,
                                           std::int64_t now) const {
  return NetworkEnvelope(
      m_config.networkId(), m_config.chainId(), m_config.protocolVersion(),
      type, m_config.localNodeId(), now, m_config.defaultTtlSeconds(), payload);
}

GossipDeliveryReport GossipMesh::broadcast(NetworkMessageType type,
                                           const std::string &payload,
                                           std::int64_t now) {
  if (!m_config.isValid()) {
    return GossipDeliveryReport(0, 1);
  }

  const NetworkEnvelope envelope = createEnvelope(type, payload, now);

  if (!envelope.isStructurallyValid(
          core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
    return GossipDeliveryReport(0, 1);
  }

  std::size_t accepted = 0;
  std::size_t rejected = 0;

  for (const PeerMetadata &peer : m_peerRegistry.activePeersAt(now)) {
    if (peer.nodeId() == m_config.localNodeId() || peer.quarantined() ||
        peer.bannedAt(now)) {
      continue;
    }

    if (requiresAuthenticatedEnvelope(type) &&
        !hasAuthenticatedOutboundSession(peer.nodeId())) {
      ++rejected;
      continue;
    }

    const OutboundQueueResult result =
        m_outboundQueue.enqueue(peer.nodeId(), envelope);

    if (result.enqueued()) {
      ++accepted;
    } else {
      ++rejected;
    }
  }

  return GossipDeliveryReport(accepted, rejected);
}

GossipDeliveryReport GossipMesh::sendTo(const std::string &targetNodeId,
                                        NetworkMessageType type,
                                        const std::string &payload,
                                        std::int64_t now) {
  if (!m_config.isValid()) {
    return GossipDeliveryReport(0, 1);
  }

  const NetworkEnvelope envelope = createEnvelope(type, payload, now);

  if (!envelope.isStructurallyValid(
          core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
    return GossipDeliveryReport(0, 1);
  }

  const PeerMetadata *peer = m_peerRegistry.peer(targetNodeId);
  if (peer == nullptr || peer->quarantined() || peer->bannedAt(now)) {
    return GossipDeliveryReport(0, 1);
  }

  if (requiresAuthenticatedEnvelope(type) &&
      !hasAuthenticatedOutboundSession(targetNodeId)) {
    return GossipDeliveryReport(0, 1);
  }

  const OutboundQueueResult result =
      m_outboundQueue.enqueue(targetNodeId, envelope);
  return result.enqueued() ? GossipDeliveryReport(1, 0)
                           : GossipDeliveryReport(0, 1);
}

void GossipMesh::injectLocalMessage(NetworkMessageType type,
                                    const std::string &payload,
                                    std::int64_t now) {
  if (!m_config.isValid())
    return;
  const NetworkEnvelope envelope = createEnvelope(type, payload, now);
  if (envelope.isStructurallyValid(
          core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
    m_inbox.add(envelope);
  }
}

GossipDeliveryReport
GossipMesh::sendHandshakeTo(const std::string &targetNodeId,
                            NetworkMessageType type, const std::string &payload,
                            std::int64_t now,
                            TransportConnectionId connectionId) {
  if (!m_config.isValid() ||
      (type != NetworkMessageType::PEER_CHALLENGE &&
       type != NetworkMessageType::PEER_HELLO) ||
      targetNodeId.empty() || targetNodeId == m_config.localNodeId()) {
    return GossipDeliveryReport(0, 1);
  }

  const NetworkEnvelope envelope = createEnvelope(type, payload, now);
  if (!envelope.isStructurallyValid(
          core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
    return GossipDeliveryReport(0, 1);
  }

  const TransportResult sent = m_transport.send(TransportMessage(
      m_config.localNodeId(), targetNodeId, envelope, now, connectionId));
  return sent.sent() ? GossipDeliveryReport(1, 0) : GossipDeliveryReport(0, 1);
}

TransportConnectionId
GossipMesh::takeConnectionIdForEnvelope(const NetworkEnvelope &envelope) {
  const auto found = m_connectionByMessageId.find(envelope.messageId());
  if (found == m_connectionByMessageId.end())
    return 0;
  const TransportConnectionId connectionId = found->second;
  m_connectionByMessageId.erase(found);
  return connectionId;
}

GossipDeliveryReport GossipMesh::flushOutbound(std::int64_t now) {
  std::size_t accepted = 0;
  std::size_t rejected = 0;

  for (const PeerMetadata &peer : m_peerRegistry.activePeers()) {
    while (m_outboundQueue.sizeForPeer(peer.nodeId()) > 0) {
      std::optional<NetworkEnvelope> envelope =
          m_outboundQueue.dequeue(peer.nodeId());

      if (!envelope.has_value()) {
        break;
      }

      if (requiresAuthenticatedEnvelope(envelope->messageType()) &&
          !hasAuthenticatedOutboundSession(peer.nodeId())) {
        ++rejected;
        continue;
      }

      TransportMessage message(m_config.localNodeId(), peer.nodeId(),
                               envelope.value(), now);

      const TransportResult result = m_transport.send(message);

      if (result.sent()) {
        ++accepted;
      } else {
        ++rejected;
        break;
      }
    }
  }

  return GossipDeliveryReport(accepted, rejected);
}

GossipDeliveryReport GossipMesh::receiveAvailable(std::int64_t now) {
  std::size_t accepted = 0;
  std::size_t rejected = 0;

  while (true) {
    std::optional<TransportMessage> message =
        m_transport.poll(m_config.localNodeId());

    if (!message.has_value()) {
      break;
    }

    if (!message->isValid()) {
      recordInvalidMessage(message->fromNodeId(), "Invalid transport message.",
                           now);
      ++rejected;
      continue;
    }

    const NetworkMessageType messageType = message->envelope().messageType();

    const PeerMetadata *senderPeer = m_peerRegistry.peer(message->fromNodeId());
    if (senderPeer != nullptr &&
        (senderPeer->quarantined() || senderPeer->bannedAt(now))) {
      (void)m_transport.disconnect(m_config.localNodeId(),
                                   message->fromNodeId());
      ++rejected;
      continue;
    }

    if (!m_rateLimiter.shouldAllow(message->fromNodeId(), messageType, now)) {
      recordInvalidMessage(
          message->fromNodeId(), "Peer exceeded per-message-type rate limit.",
          now, PeerMisbehaviorType::RATE_LIMIT_EXCEEDED, &message->envelope());
      ++rejected;
      continue;
    }

    // Reject envelopes whose declared sender does not match the authenticated
    // transport sender. Without this check, any connected peer can claim to be
    // another node id and bypass per-sender rate limits.
    if (message->envelope().senderNodeId() != message->fromNodeId()) {
      recordInvalidMessage(
          message->fromNodeId(),
          "Envelope sender id does not match transport sender.", now,
          PeerMisbehaviorType::INVALID_MESSAGE, &message->envelope());
      ++rejected;
      continue;
    }

    const bool isHandshake =
        (messageType == NetworkMessageType::PEER_CHALLENGE ||
         messageType == NetworkMessageType::PEER_HELLO);

    if (!isHandshake) {
      if (!m_peerRegistry.contains(message->fromNodeId())) {
        recordInvalidMessage(
            message->fromNodeId(), "Unknown peer sent a non-handshake message.",
            now, PeerMisbehaviorType::INVALID_MESSAGE, &message->envelope());
        ++rejected;
        continue;
      }
    }

    if (requiresAuthenticatedEnvelope(messageType)) {
      if (!hasAuthenticatedInboundSession(message->fromNodeId())) {
        recordInvalidMessage(message->fromNodeId(),
                             "Peer sent non-handshake message without an "
                             "authenticated inbound session.",
                             now, PeerMisbehaviorType::INVALID_MESSAGE,
                             &message->envelope());
        ++rejected;
        continue;
      }
    }

    // Stateless envelope identity checks stay at the mesh so a peer speaking
    // for the wrong network/chain/protocol feeds quarantine and evidence
    // capture even when no orchestrator drives this mesh. Stateful policy
    // (dedup, per-type payload limits, per-peer windows) lives in the
    // NodeOrchestrator inbound validation path.
    const NetworkEnvelope &envelope = message->envelope();
    std::string identityMismatchReason;
    if (!envelope.isStructurallyValid(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
      identityMismatchReason = "Network envelope failed structural validation.";
    } else if (envelope.networkId() != m_config.networkId()) {
      identityMismatchReason = "Network id mismatch.";
    } else if (envelope.chainId() != m_config.chainId()) {
      identityMismatchReason = "Chain id mismatch.";
    } else if (envelope.protocolVersion() != m_config.protocolVersion()) {
      identityMismatchReason = "Protocol version mismatch.";
    }
    if (!identityMismatchReason.empty()) {
      recordInvalidMessage(message->fromNodeId(), identityMismatchReason, now,
                           PeerMisbehaviorType::INVALID_MESSAGE,
                           &message->envelope());
      ++rejected;
      continue;
    }

    m_peerRegistry.updateHeartbeat(message->fromNodeId(), now);
    if (message->hasConnectionId() &&
        (messageType == NetworkMessageType::PEER_CHALLENGE ||
         messageType == NetworkMessageType::PEER_HELLO)) {
      m_connectionByMessageId[message->envelope().messageId()] =
          message->connectionId();
    }
    m_inbox.add(message->envelope());
    ++accepted;
  }

  return GossipDeliveryReport(accepted, rejected);
}

std::size_t
GossipMesh::invalidMessageCountForPeer(const std::string &nodeId) const {
  const auto found =
      m_invalidMessagesByIdentity.find(identityKeyForNodeId(nodeId));

  if (found == m_invalidMessagesByIdentity.end()) {
    return 0;
  }

  return found->second;
}

std::uint32_t
GossipMesh::rateLimitedMessageCountForPeer(const std::string &nodeId,
                                           std::int64_t now) const {
  return m_rateLimiter.messageCount(nodeId, now);
}

std::size_t
GossipMesh::quarantineEscalationForPeer(const std::string &nodeId) const {
  const std::string identityKey = identityKeyForNodeId(nodeId);
  auto it = m_quarantineEscalationByIdentity.find(identityKey);
  return it != m_quarantineEscalationByIdentity.end() ? it->second : 0;
}

void GossipMesh::reportPeerMisbehavior(const NetworkEnvelope &envelope,
                                       PeerMisbehaviorType type,
                                       const std::string &reason,
                                       std::int64_t now) {
  recordInvalidMessage(envelope.senderNodeId(), reason, now, type, &envelope);
}

bool GossipMesh::restorePeerPenaltyState(const std::string &nodeId,
                                         std::size_t invalidMessageCount,
                                         std::size_t escalationCount,
                                         std::int32_t score) {
  return restorePeerPenaltyState(nodeId, invalidMessageCount, escalationCount,
                                 0, "", 0, score);
}

bool GossipMesh::restorePeerPenaltyState(const std::string &nodeId,
                                         std::size_t invalidMessageCount,
                                         std::size_t escalationCount,
                                         std::int64_t bannedUntil,
                                         const std::string &banReason,
                                         std::int64_t now, std::int32_t score) {
  if (!m_peerRegistry.contains(nodeId)) {
    return false;
  }

  const std::string identityKey = identityKeyForNodeId(nodeId);
  auto lruIt = m_lruInvalidMessagesMap.find(identityKey);
  if (lruIt != m_lruInvalidMessagesMap.end()) {
    m_lruInvalidMessages.erase(lruIt->second);
  }
  m_lruInvalidMessages.push_front(identityKey);
  m_lruInvalidMessagesMap[identityKey] = m_lruInvalidMessages.begin();

  m_invalidMessagesByIdentity[identityKey] = std::min(
      invalidMessageCount, m_config.invalidMessageQuarantineThreshold());
  m_quarantineEscalationByIdentity[identityKey] = escalationCount;

  while (m_lruInvalidMessages.size() > MAX_INVALID_TRACKING) {
    const std::string evicted = m_lruInvalidMessages.back();
    m_lruInvalidMessages.pop_back();
    m_lruInvalidMessagesMap.erase(evicted);
    m_invalidMessagesByIdentity.erase(evicted);
  }

  std::int32_t currentScore = m_peerRegistry.peer(nodeId)->score();
  if (currentScore != score) {
    m_peerRegistry.adjustScore(nodeId, score - currentScore, "restored");
  }

  if (bannedUntil > 0 && (now <= 0 || bannedUntil > now)) {
    m_peerRegistry.banPeer(
        nodeId, bannedUntil,
        banReason.empty() ? "Peer temporary ban restored from persistent state."
                          : banReason);
  } else if (shouldQuarantinePeer(nodeId)) {
    m_peerRegistry.quarantinePeer(
        nodeId, "Peer quarantine restored from persistent state.");
  } else if (bannedUntil > 0 && now > 0 && bannedUntil <= now) {
    m_peerRegistry.liftPeerPenalty(nodeId, now);
    m_invalidMessagesByIdentity[identityKeyForNodeId(nodeId)] = 0;
  }
  return true;
}

std::size_t GossipMesh::liftExpiredPeerPenalties(std::int64_t now) {
  if (now <= 0) {
    return 0;
  }

  std::vector<std::string> expiredIdentityKeys;
  for (const PeerMetadata &peer : m_peerRegistry.allPeers()) {
    if (peer.quarantined() && peer.bannedUntil() > 0 &&
        peer.bannedUntil() <= now) {
      expiredIdentityKeys.push_back(peer.identityKey());
    }
  }

  const std::size_t lifted = m_peerRegistry.liftExpiredPeerPenalties(now);
  if (lifted > 0) {
    for (const std::string &identityKey : expiredIdentityKeys) {
      m_invalidMessagesByIdentity[identityKey] = 0;
    }
    persistPeerPenaltyState();
  }
  return lifted;
}

bool GossipMesh::peerBannedAt(const std::string &nodeId,
                              std::int64_t now) const {
  const PeerMetadata *peer = m_peerRegistry.peer(nodeId);
  return peer != nullptr && peer->bannedAt(now);
}

bool GossipMesh::isIpQuarantined(const std::string &ip) const {
  if (ip.empty()) {
    return false;
  }
  for (const PeerMetadata &peer : m_peerRegistry.allPeers()) {
    if (peer.quarantined() && peer.endpoint().host() == ip) {
      return true;
    }
  }
  return false;
}

void GossipMesh::setPeerPenaltyPersistenceHandler(
    std::function<void()> handler) {
  m_peerPenaltyPersistenceHandler = std::move(handler);
}

bool GossipMesh::peerPenaltyPersistenceHealthy() const {
  return m_lastPeerPenaltyPersistenceError.empty();
}

const std::string &GossipMesh::lastPeerPenaltyPersistenceError() const {
  return m_lastPeerPenaltyPersistenceError;
}

bool GossipMesh::shouldQuarantinePeer(const std::string &nodeId) const {
  return invalidMessageCountForPeer(nodeId) >=
         m_config.invalidMessageQuarantineThreshold();
}

std::string GossipMesh::identityKeyForNodeId(const std::string &nodeId) const {
  const PeerMetadata *peer = m_peerRegistry.peer(nodeId);
  return peer == nullptr ? "unverified-node-" + nodeId : peer->identityKey();
}

void GossipMesh::recordInvalidMessage(const std::string &nodeId,
                                      const std::string &reason,
                                      std::int64_t now,
                                      PeerMisbehaviorType type,
                                      const NetworkEnvelope *envelope) {
  if (nodeId.empty()) {
    return;
  }

  if (envelope != nullptr) {
    bool stillAllowed = m_rateLimiter.recordInvalidMessage(
        nodeId, envelope->messageType(), now);
    if (!stillAllowed && type != PeerMisbehaviorType::RATE_LIMIT_EXCEEDED) {
      type = PeerMisbehaviorType::RATE_LIMIT_EXCEEDED;
    }
  }

  const std::string identityKey = identityKeyForNodeId(nodeId);

  auto lruIt = m_lruInvalidMessagesMap.find(identityKey);
  if (lruIt != m_lruInvalidMessagesMap.end()) {
    m_lruInvalidMessages.erase(lruIt->second);
  }
  m_lruInvalidMessages.push_front(identityKey);
  m_lruInvalidMessagesMap[identityKey] = m_lruInvalidMessages.begin();

  std::size_t &invalidCount = m_invalidMessagesByIdentity[identityKey];
  if (invalidCount < m_config.invalidMessageQuarantineThreshold()) {
    ++invalidCount;
  }

  while (m_lruInvalidMessages.size() > MAX_INVALID_TRACKING) {
    const std::string evicted = m_lruInvalidMessages.back();
    m_lruInvalidMessages.pop_back();
    m_lruInvalidMessagesMap.erase(evicted);
    m_invalidMessagesByIdentity.erase(evicted);
  }

  const PeerMetadata *peerMeta = m_peerRegistry.peer(nodeId);
  const bool wasQuarantinedBefore =
      peerMeta != nullptr && peerMeta->quarantined();
  const bool wasBannedBefore = peerMeta != nullptr && peerMeta->bannedAt(now);
  bool peerStateChanged = false;

  if (peerMeta != nullptr && !wasBannedBefore) {
    const std::int32_t scorePenalty =
        type == PeerMisbehaviorType::RATE_LIMIT_EXCEEDED
            ? kRateLimitScorePenalty
            : kInvalidMessageScorePenalty;
    peerStateChanged =
        m_peerRegistry.adjustScore(nodeId, scorePenalty, reason).success();
  }

  const PeerMetadata *scoredPeer = m_peerRegistry.peer(nodeId);
  const bool crossedBanBoundary =
      scoredPeer != nullptr &&
      (shouldQuarantinePeer(nodeId) ||
       scoredPeer->score() <= kTemporaryBanScoreThreshold);
  if (crossedBanBoundary && !wasBannedBefore &&
      m_peerRegistry.contains(nodeId)) {
    m_quarantineEscalationByIdentity[identityKey]++;
    const std::size_t escalation =
        m_quarantineEscalationByIdentity[identityKey];
    std::int64_t durationSeconds = 60; // 1 min
    if (escalation == 2) {
      durationSeconds = 300; // 5 min
    } else if (escalation == 3) {
      durationSeconds = 1800; // 30 min
    } else if (escalation >= 4) {
      durationSeconds = 30 * 86400; // 30 days (session ban)
    }

    const std::int64_t bannedUntil = now + durationSeconds;
    peerStateChanged =
        m_peerRegistry.banPeer(nodeId, bannedUntil, reason).success() ||
        peerStateChanged;
  }

  const PeerMetadata *updatedPeer = m_peerRegistry.peer(nodeId);
  const bool newlyBanned =
      !wasBannedBefore && updatedPeer != nullptr && updatedPeer->bannedAt(now);
  const bool newlyQuarantined = !wasQuarantinedBefore &&
                                updatedPeer != nullptr &&
                                updatedPeer->quarantined();

  if (newlyBanned || newlyQuarantined) {
    (void)m_transport.disconnect(m_config.localNodeId(), nodeId);
  }

  if (peerStateChanged || !peerPenaltyPersistenceHealthy()) {
    persistPeerPenaltyState();
  }

  if (m_evidenceStore == nullptr || m_config.localNodeId().empty()) {
    return;
  }

  // Determine evidence type from context.
  economics::ProtocolEvidenceType evidenceType;
  std::string ruleId;
  if (newlyBanned) {
    evidenceType = economics::ProtocolEvidenceType::P2P_PEER_BANNED;
    ruleId = "p2p.temporary-ban";
  } else if (newlyQuarantined) {
    evidenceType = economics::ProtocolEvidenceType::P2P_PEER_QUARANTINED;
    ruleId = "p2p.quarantine.threshold";
  } else if (type == PeerMisbehaviorType::RATE_LIMIT_EXCEEDED) {
    evidenceType = economics::ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED;
    ruleId = "p2p.rate-limit";
  } else {
    evidenceType = economics::ProtocolEvidenceType::P2P_INVALID_MESSAGE;
    ruleId = "p2p.inbound-validation";
  }

  // Coalesce by cryptographic identity so changing a display node id cannot
  // bypass evidence throttling.
  constexpr std::int64_t kCoalescingWindowSeconds = 60;
  constexpr std::size_t kMaxCoalescingEntries = 4096;
  const std::pair<std::string, std::string> coalescingKey = {identityKey,
                                                             ruleId};
  const auto lastIt = m_lastEvidenceAt.find(coalescingKey);
  if (lastIt != m_lastEvidenceAt.end() &&
      now - lastIt->second < kCoalescingWindowSeconds) {
    return;
  }

  // Evict stale entries when the table grows too large to prevent unbounded
  // memory growth over long node uptimes.
  if (m_lastEvidenceAt.size() >= kMaxCoalescingEntries) {
    const std::int64_t cutoff = now - kCoalescingWindowSeconds;
    auto it = m_lastEvidenceAt.begin();
    while (it != m_lastEvidenceAt.end()) {
      if (it->second < cutoff) {
        it = m_lastEvidenceAt.erase(it);
      } else {
        ++it;
      }
    }
    // If still over limit after TTL eviction, we are likely under a flood.
    // Return early to prevent unbounded O(N) eviction scans and protect disk
    // IO.
    if (m_lastEvidenceAt.size() >= kMaxCoalescingEntries) {
      return;
    }
  }

  m_lastEvidenceAt[coalescingKey] = now;

  // Bind evidence to the offending payload when an application layer
  // supplies the original envelope; transport failures fall back to reason.
  char digestBuf[NODO_HASH_BUFFER_SIZE] = {};
  const std::string digestSource =
      envelope == nullptr ? reason : envelope->payload();
  nodo_hash_string(digestSource.c_str(), digestBuf, NODO_HASH_BUFFER_SIZE);
  const std::string payloadDigest(digestBuf);

  // Evidence id encodes subject, type, and timestamp to be unique per event.
  const std::string evidenceIdRaw =
      "p2p-evidence:" + identityKey + ":" + ruleId + ":" + std::to_string(now);
  char idBuf[NODO_HASH_BUFFER_SIZE] = {};
  nodo_hash_string(evidenceIdRaw.c_str(), idBuf, NODO_HASH_BUFFER_SIZE);
  const std::string evidenceId(idBuf);

  const economics::ProtocolEvidence evidence(
      evidenceId, evidenceType, identityKey, m_config.localNodeId(),
      0, // blockHeight unknown at P2P layer
      0, // epoch unknown at P2P layer
      ruleId, payloadDigest, reason.substr(0, 512), now);

  if (!evidence.isValid()) {
    return;
  }

  try {
    m_evidenceStore->save(evidence);
    m_evidenceCaptureHealth.recordSuccess();
  } catch (const std::exception &e) {
    // Evidence store failure is tracked through diagnostics but does not crash
    // the mesh. The invalid message counter above tracks the violation in
    // memory.
    m_evidenceCaptureHealth.recordFailure(e.what(), now);
  } catch (...) {
    m_evidenceCaptureHealth.recordFailure("unknown exception in evidence store",
                                          now);
  }
}

void GossipMesh::persistPeerPenaltyState() {
  if (!m_peerPenaltyPersistenceHandler) {
    return;
  }

  try {
    m_peerPenaltyPersistenceHandler();
    m_lastPeerPenaltyPersistenceError.clear();
  } catch (const std::exception &error) {
    m_lastPeerPenaltyPersistenceError = error.what();
  } catch (...) {
    m_lastPeerPenaltyPersistenceError =
        "Unknown exception while persisting peer penalty state.";
  }
}

} // namespace nodo::p2p
