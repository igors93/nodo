#include "p2p/GossipMesh.hpp"

#include "core/ProtocolLimits.hpp"

#include "crypto/hash.h"
#include "economics/ProtocolEvidence.hpp"
#include "storage/ProtocolEvidenceStore.hpp"

#include <stdexcept>

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

constexpr std::int32_t kInvalidMessageScorePenalty = -10;
constexpr std::int32_t kRateLimitScorePenalty = -5;

bool isSafeScalar(const std::string& value) {
    if (value.empty() || value.size() > 200) {
        return false;
    }

    for (const char character : value) {
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

} // namespace

GossipMeshConfig::GossipMeshConfig()
    : m_localNodeId(""),
      m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_genesisId(""),
      m_defaultTtlSeconds(0),
      m_invalidMessageQuarantineThreshold(0) {}

GossipMeshConfig::GossipMeshConfig(
    std::string localNodeId,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::string genesisId,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold
) : m_localNodeId(std::move(localNodeId)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_genesisId(std::move(genesisId)),
    m_defaultTtlSeconds(defaultTtlSeconds),
    m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold) {}

const std::string& GossipMeshConfig::localNodeId() const { return m_localNodeId; }
const std::string& GossipMeshConfig::networkId() const { return m_networkId; }
const std::string& GossipMeshConfig::chainId() const { return m_chainId; }
const std::string& GossipMeshConfig::protocolVersion() const { return m_protocolVersion; }
const std::string& GossipMeshConfig::genesisId() const { return m_genesisId; }
std::uint32_t GossipMeshConfig::defaultTtlSeconds() const { return m_defaultTtlSeconds; }
std::size_t GossipMeshConfig::invalidMessageQuarantineThreshold() const { return m_invalidMessageQuarantineThreshold; }

bool GossipMeshConfig::isValid() const {
    return isSafeScalar(m_localNodeId) &&
           isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
           !m_genesisId.empty() &&
           m_defaultTtlSeconds > 0 &&
           m_defaultTtlSeconds <= 3600 &&
           m_invalidMessageQuarantineThreshold > 0;
}

GossipDeliveryReport::GossipDeliveryReport()
    : m_acceptedCount(0),
      m_rejectedCount(0) {}

GossipDeliveryReport::GossipDeliveryReport(
    std::size_t acceptedCount,
    std::size_t rejectedCount
) : m_acceptedCount(acceptedCount),
    m_rejectedCount(rejectedCount) {}

std::size_t GossipDeliveryReport::acceptedCount() const { return m_acceptedCount; }
std::size_t GossipDeliveryReport::rejectedCount() const { return m_rejectedCount; }
bool GossipDeliveryReport::allAccepted() const { return m_rejectedCount == 0; }

std::string GossipDeliveryReport::serialize() const {
    std::ostringstream output;
    output << "GossipDeliveryReport{acceptedCount=" << m_acceptedCount
           << ";rejectedCount=" << m_rejectedCount
           << "}";
    return output.str();
}

GossipInbox::GossipInbox()
    : m_messagesByType() {}

void GossipInbox::add(const NetworkEnvelope& envelope) {
    m_messagesByType[envelope.messageType()].push_back(envelope);
}

std::size_t GossipInbox::totalCount() const {
    std::size_t total = 0;

    for (const auto& [type, messages] : m_messagesByType) {
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

std::vector<NetworkEnvelope> GossipInbox::messagesForType(NetworkMessageType type) const {
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

    for (auto& [type, messages] : m_messagesByType) {
        (void)type;
        all.insert(all.end(),
                   std::make_move_iterator(messages.begin()),
                   std::make_move_iterator(messages.end()));
    }

    m_messagesByType.clear();
    return all;
}

std::string GossipInbox::serialize() const {
    std::ostringstream output;
    output << "GossipInbox{totalCount=" << totalCount() << ";types=[";

    bool first = true;
    for (const auto& [type, messages] : m_messagesByType) {
        if (!first) {
            output << ",";
        }
        output << networkMessageTypeToString(type) << ":" << messages.size();
        first = false;
    }

    output << "]}";
    return output.str();
}

GossipMesh::GossipMesh(
    GossipMeshConfig config,
    Transport& transport
) : m_config(std::move(config)),
    m_transport(transport),
    m_evidenceStore(nullptr),
    m_peerRegistry(),
    m_outboundQueue(1024),
    m_inboundValidator(),
    m_rateLimiter(),
    m_inbox(),
    m_invalidMessagesByIdentity(),
    m_lastEvidenceAt(),
    m_evidenceCaptureHealth(),
    m_peerPenaltyPersistenceHandler(),
    m_lastPeerPenaltyPersistenceError()
{
    m_evidenceCaptureHealth.markUnavailable();
}

GossipMesh::GossipMesh(
    GossipMeshConfig config,
    Transport& transport,
    storage::ProtocolEvidenceStore* evidenceStore
) : m_config(std::move(config)),
    m_transport(transport),
    m_evidenceStore(evidenceStore),
    m_peerRegistry(),
    m_outboundQueue(1024),
    m_inboundValidator(),
    m_rateLimiter(),
    m_inbox(),
    m_invalidMessagesByIdentity(),
    m_lastEvidenceAt(),
    m_evidenceCaptureHealth(),
    m_peerPenaltyPersistenceHandler(),
    m_lastPeerPenaltyPersistenceError()
{
    if (evidenceStore == nullptr) {
        m_evidenceCaptureHealth.markUnavailable();
    }
}

const GossipMeshConfig& GossipMesh::config() const { return m_config; }
PeerRegistry& GossipMesh::peerRegistry() { return m_peerRegistry; }
const PeerRegistry& GossipMesh::peerRegistry() const { return m_peerRegistry; }
const GossipInbox& GossipMesh::inbox() const { return m_inbox; }

std::vector<NetworkEnvelope> GossipMesh::drainInbox(NetworkMessageType type) {
    return m_inbox.drain(type);
}

const node::EvidenceCaptureHealth& GossipMesh::evidenceCaptureHealth() const {
    return m_evidenceCaptureHealth;
}

PeerRegistryResult GossipMesh::registerPeer(PeerMetadata peer) {
    if (!m_config.isValid()) {
        return PeerRegistryResult(
            PeerRegistryStatus::REJECTED,
            "Gossip mesh config is invalid."
        );
    }

    if (peer.nodeId() == m_config.localNodeId()) {
        return PeerRegistryResult(
            PeerRegistryStatus::REJECTED,
            "Cannot register local node as remote peer."
        );
    }

    return m_peerRegistry.registerPeer(std::move(peer));
}

TransportResult GossipMesh::connectPeer(const std::string& remoteNodeId) {
    const PeerMetadata* peer = m_peerRegistry.peer(remoteNodeId);
    if (peer == nullptr) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Cannot connect unknown peer."
        );
    }

    if (peer->quarantined()) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Cannot connect quarantined peer."
        );
    }

    return m_transport.connect(m_config.localNodeId(), remoteNodeId);
}

TransportResult GossipMesh::disconnectPeer(const std::string& remoteNodeId) {
    return m_transport.disconnect(m_config.localNodeId(), remoteNodeId);
}

NetworkEnvelope GossipMesh::createEnvelope(
    NetworkMessageType type,
    const std::string& payload,
    std::int64_t now
) const {
    return NetworkEnvelope(
        m_config.networkId(),
        m_config.chainId(),
        m_config.protocolVersion(),
        type,
        m_config.localNodeId(),
        now,
        m_config.defaultTtlSeconds(),
        payload
    );
}

GossipDeliveryReport GossipMesh::broadcast(
    NetworkMessageType type,
    const std::string& payload,
    std::int64_t now
) {
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

    for (const PeerMetadata& peer : m_peerRegistry.activePeers()) {
        if (peer.nodeId() == m_config.localNodeId() || peer.quarantined()) {
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

GossipDeliveryReport GossipMesh::sendTo(
    const std::string& targetNodeId,
    NetworkMessageType type,
    const std::string& payload,
    std::int64_t now
) {
    if (!m_config.isValid()) {
        return GossipDeliveryReport(0, 1);
    }

    const NetworkEnvelope envelope = createEnvelope(type, payload, now);

    if (!envelope.isStructurallyValid(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
        return GossipDeliveryReport(0, 1);
    }

    const PeerMetadata* peer = m_peerRegistry.peer(targetNodeId);
    if (peer == nullptr || peer->quarantined()) {
        return GossipDeliveryReport(0, 1);
    }

    const OutboundQueueResult result = m_outboundQueue.enqueue(targetNodeId, envelope);
    return result.enqueued() ? GossipDeliveryReport(1, 0) : GossipDeliveryReport(0, 1);
}

GossipDeliveryReport GossipMesh::sendHandshakeTo(
    const std::string& targetNodeId,
    const std::string& payload,
    std::int64_t now
) {
    if (!m_config.isValid() || targetNodeId.empty() ||
        targetNodeId == m_config.localNodeId()) {
        return GossipDeliveryReport(0, 1);
    }

    const NetworkEnvelope envelope = createEnvelope(
        NetworkMessageType::PEER_HELLO,
        payload,
        now
    );
    if (!envelope.isStructurallyValid(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
        return GossipDeliveryReport(0, 1);
    }

    const TransportResult sent = m_transport.send(TransportMessage(
        m_config.localNodeId(),
        targetNodeId,
        envelope,
        now
    ));
    return sent.sent()
        ? GossipDeliveryReport(1, 0)
        : GossipDeliveryReport(0, 1);
}

GossipDeliveryReport GossipMesh::flushOutbound(std::int64_t now) {
    std::size_t accepted = 0;
    std::size_t rejected = 0;

    for (const PeerMetadata& peer : m_peerRegistry.activePeers()) {
        while (m_outboundQueue.sizeForPeer(peer.nodeId()) > 0) {
            std::optional<NetworkEnvelope> envelope =
                m_outboundQueue.dequeue(peer.nodeId());

            if (!envelope.has_value()) {
                break;
            }

            TransportMessage message(
                m_config.localNodeId(),
                peer.nodeId(),
                envelope.value(),
                now
            );

            const TransportResult result =
                m_transport.send(message);

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
            recordInvalidMessage(message->fromNodeId(), "Invalid transport message.", now);
            ++rejected;
            continue;
        }

        const PeerMetadata* senderPeer =
            m_peerRegistry.peer(message->fromNodeId());
        if (senderPeer != nullptr && senderPeer->quarantined()) {
            ++rejected;
            continue;
        }

        if (!m_rateLimiter.shouldAllow(message->fromNodeId(), now)) {
            recordInvalidMessage(message->fromNodeId(), "Peer exceeded message rate limit.", now);
            ++rejected;
            continue;
        }

        // Reject envelopes whose declared sender does not match the authenticated
        // transport sender. Without this check, any connected peer can claim to be
        // another node id and bypass per-sender rate limits.
        if (message->envelope().senderNodeId() != message->fromNodeId()) {
            recordInvalidMessage(message->fromNodeId(), "Envelope sender id does not match transport sender.", now);
            ++rejected;
            continue;
        }

        const InboundMessageResult validation =
            m_inboundValidator.validate(
                message->envelope(),
                m_config.networkId(),
                m_config.chainId(),
                m_config.protocolVersion(),
                now
            );

        if (!validation.accepted()) {
            recordInvalidMessage(message->fromNodeId(), validation.reason(), now);
            ++rejected;
            continue;
        }

        if (!m_peerRegistry.contains(message->fromNodeId()) &&
            message->envelope().messageType() !=
                NetworkMessageType::PEER_HELLO) {
            recordInvalidMessage(
                message->fromNodeId(),
                "Unauthenticated peer sent a non-handshake message.",
                now
            );
            ++rejected;
            continue;
        }

        m_peerRegistry.updateHeartbeat(message->fromNodeId(), now);
        m_inbox.add(message->envelope());
        ++accepted;
    }

    return GossipDeliveryReport(accepted, rejected);
}

std::size_t GossipMesh::invalidMessageCountForPeer(const std::string& nodeId) const {
    const auto found = m_invalidMessagesByIdentity.find(
        identityKeyForNodeId(nodeId)
    );

    if (found == m_invalidMessagesByIdentity.end()) {
        return 0;
    }

    return found->second;
}

std::uint32_t GossipMesh::rateLimitedMessageCountForPeer(
    const std::string& nodeId,
    std::int64_t now
) const {
    return m_rateLimiter.messageCount(nodeId, now);
}

void GossipMesh::reportPeerMisbehavior(
    const NetworkEnvelope& envelope,
    PeerMisbehaviorType type,
    const std::string& reason,
    std::int64_t now
) {
    recordInvalidMessage(
        envelope.senderNodeId(),
        reason,
        now,
        type,
        &envelope
    );
}

bool GossipMesh::restorePeerPenaltyState(
    const std::string& nodeId,
    std::size_t invalidMessageCount
) {
    if (!m_peerRegistry.contains(nodeId)) {
        return false;
    }

    m_invalidMessagesByIdentity[identityKeyForNodeId(nodeId)] = std::min(
        invalidMessageCount,
        m_config.invalidMessageQuarantineThreshold()
    );
    if (shouldQuarantinePeer(nodeId)) {
        m_peerRegistry.quarantinePeer(
            nodeId,
            "Peer quarantine restored from persistent state."
        );
    }
    return true;
}

void GossipMesh::setPeerPenaltyPersistenceHandler(
    std::function<void()> handler
) {
    m_peerPenaltyPersistenceHandler = std::move(handler);
}

bool GossipMesh::peerPenaltyPersistenceHealthy() const {
    return m_lastPeerPenaltyPersistenceError.empty();
}

const std::string& GossipMesh::lastPeerPenaltyPersistenceError() const {
    return m_lastPeerPenaltyPersistenceError;
}

bool GossipMesh::shouldQuarantinePeer(const std::string& nodeId) const {
    return invalidMessageCountForPeer(nodeId) >=
           m_config.invalidMessageQuarantineThreshold();
}

std::string GossipMesh::identityKeyForNodeId(
    const std::string& nodeId
) const {
    const PeerMetadata* peer = m_peerRegistry.peer(nodeId);
    return peer == nullptr
        ? "unverified-node-" + nodeId
        : peer->identityKey();
}

void GossipMesh::recordInvalidMessage(
    const std::string& nodeId,
    const std::string& reason,
    std::int64_t now,
    PeerMisbehaviorType type,
    const NetworkEnvelope* envelope
) {
    if (nodeId.empty()) {
        return;
    }

    const std::string identityKey = identityKeyForNodeId(nodeId);
    std::size_t& invalidCount =
        m_invalidMessagesByIdentity[identityKey];
    if (invalidCount < m_config.invalidMessageQuarantineThreshold()) {
        ++invalidCount;
    }

    const PeerMetadata* peerMeta = m_peerRegistry.peer(nodeId);
    const bool wasQuarantinedBefore = peerMeta != nullptr && peerMeta->quarantined();
    bool peerStateChanged = false;

    if (peerMeta != nullptr && !wasQuarantinedBefore) {
        const std::int32_t scorePenalty =
            type == PeerMisbehaviorType::RATE_LIMIT_EXCEEDED
                ? kRateLimitScorePenalty
                : kInvalidMessageScorePenalty;
        peerStateChanged = m_peerRegistry.adjustScore(
            nodeId,
            scorePenalty,
            reason
        ).success();
    }

    if (shouldQuarantinePeer(nodeId) && m_peerRegistry.contains(nodeId)) {
        if (!wasQuarantinedBefore) {
            peerStateChanged =
                m_peerRegistry.quarantinePeer(nodeId, reason).success() ||
                peerStateChanged;
        }
    }

    const PeerMetadata* updatedPeer = m_peerRegistry.peer(nodeId);
    const bool newlyQuarantined =
        !wasQuarantinedBefore && updatedPeer != nullptr &&
        updatedPeer->quarantined();

    if (peerStateChanged || !peerPenaltyPersistenceHealthy()) {
        persistPeerPenaltyState();
    }

    if (m_evidenceStore == nullptr || m_config.localNodeId().empty()) {
        return;
    }

    // Determine evidence type from context.
    economics::ProtocolEvidenceType evidenceType;
    std::string ruleId;
    if (newlyQuarantined) {
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
    const std::pair<std::string, std::string> coalescingKey =
        {identityKey, ruleId};
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
        // Return early to prevent unbounded O(N) eviction scans and protect disk IO.
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
        "p2p-evidence:" + identityKey + ":" + ruleId + ":" +
        std::to_string(now);
    char idBuf[NODO_HASH_BUFFER_SIZE] = {};
    nodo_hash_string(evidenceIdRaw.c_str(), idBuf, NODO_HASH_BUFFER_SIZE);
    const std::string evidenceId(idBuf);

    const economics::ProtocolEvidence evidence(
        evidenceId,
        evidenceType,
        identityKey,
        m_config.localNodeId(),
        0,   // blockHeight unknown at P2P layer
        0,   // epoch unknown at P2P layer
        ruleId,
        payloadDigest,
        reason.substr(0, 512),
        now
    );

    if (!evidence.isValid()) {
        return;
    }

    try {
        m_evidenceStore->save(evidence);
        m_evidenceCaptureHealth.recordSuccess();
    } catch (const std::exception& e) {
        // Evidence store failure is tracked through diagnostics but does not crash
        // the mesh. The invalid message counter above tracks the violation in memory.
        m_evidenceCaptureHealth.recordFailure(e.what(), now);
    } catch (...) {
        m_evidenceCaptureHealth.recordFailure("unknown exception in evidence store", now);
    }
}

void GossipMesh::persistPeerPenaltyState() {
    if (!m_peerPenaltyPersistenceHandler) {
        return;
    }

    try {
        m_peerPenaltyPersistenceHandler();
        m_lastPeerPenaltyPersistenceError.clear();
    } catch (const std::exception& error) {
        m_lastPeerPenaltyPersistenceError = error.what();
    } catch (...) {
        m_lastPeerPenaltyPersistenceError =
            "Unknown exception while persisting peer penalty state.";
    }
}

} // namespace nodo::p2p
