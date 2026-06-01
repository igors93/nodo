#include "p2p/GossipMesh.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

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
      m_defaultTtlSeconds(0),
      m_invalidMessageQuarantineThreshold(0) {}

GossipMeshConfig::GossipMeshConfig(
    std::string localNodeId,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold
) : m_localNodeId(std::move(localNodeId)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_defaultTtlSeconds(defaultTtlSeconds),
    m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold) {}

const std::string& GossipMeshConfig::localNodeId() const { return m_localNodeId; }
const std::string& GossipMeshConfig::networkId() const { return m_networkId; }
const std::string& GossipMeshConfig::chainId() const { return m_chainId; }
const std::string& GossipMeshConfig::protocolVersion() const { return m_protocolVersion; }
std::uint32_t GossipMeshConfig::defaultTtlSeconds() const { return m_defaultTtlSeconds; }
std::size_t GossipMeshConfig::invalidMessageQuarantineThreshold() const { return m_invalidMessageQuarantineThreshold; }

bool GossipMeshConfig::isValid() const {
    return isSafeScalar(m_localNodeId) &&
           isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
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
    m_peerRegistry(),
    m_outboundQueue(1024),
    m_inboundValidator(),
    m_rateLimiter(),
    m_inbox(),
    m_invalidMessagesByPeer() {}

const GossipMeshConfig& GossipMesh::config() const { return m_config; }
PeerRegistry& GossipMesh::peerRegistry() { return m_peerRegistry; }
const PeerRegistry& GossipMesh::peerRegistry() const { return m_peerRegistry; }
const GossipInbox& GossipMesh::inbox() const { return m_inbox; }

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
    if (!m_peerRegistry.contains(remoteNodeId)) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Cannot connect unknown peer."
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

    if (!envelope.isStructurallyValid(1024 * 1024)) {
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
            recordInvalidMessage(message->fromNodeId(), "Invalid transport message.");
            ++rejected;
            continue;
        }

        if (!m_rateLimiter.shouldAllow(message->fromNodeId(), now)) {
            recordInvalidMessage(message->fromNodeId(), "Peer exceeded message rate limit.");
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
            recordInvalidMessage(message->fromNodeId(), validation.reason());
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
    const auto found = m_invalidMessagesByPeer.find(nodeId);

    if (found == m_invalidMessagesByPeer.end()) {
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

bool GossipMesh::shouldQuarantinePeer(const std::string& nodeId) const {
    return invalidMessageCountForPeer(nodeId) >=
           m_config.invalidMessageQuarantineThreshold();
}

void GossipMesh::recordInvalidMessage(
    const std::string& nodeId,
    const std::string& reason
) {
    if (nodeId.empty()) {
        return;
    }

    m_invalidMessagesByPeer[nodeId] += 1;

    if (shouldQuarantinePeer(nodeId) && m_peerRegistry.contains(nodeId)) {
        m_peerRegistry.quarantinePeer(nodeId, reason);
    }
}

} // namespace nodo::p2p
