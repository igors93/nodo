#include "p2p/PeerHandshakeManager.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

PeerHelloMessage::PeerHelloMessage()
    : m_peer(),
      m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_chainStatus(),
      m_createdAt(0) {}

PeerHelloMessage::PeerHelloMessage(
    PeerMetadata peer,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    node::ChainStatusMessage chainStatus,
    std::int64_t createdAt
) : m_peer(std::move(peer)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_chainStatus(std::move(chainStatus)),
    m_createdAt(createdAt) {}

const PeerMetadata& PeerHelloMessage::peer() const { return m_peer; }
const std::string& PeerHelloMessage::networkId() const { return m_networkId; }
const std::string& PeerHelloMessage::chainId() const { return m_chainId; }
const std::string& PeerHelloMessage::protocolVersion() const { return m_protocolVersion; }
const node::ChainStatusMessage& PeerHelloMessage::chainStatus() const { return m_chainStatus; }
std::int64_t PeerHelloMessage::createdAt() const { return m_createdAt; }

bool PeerHelloMessage::isValid() const {
    return m_peer.isValid() &&
           !m_networkId.empty() &&
           !m_chainId.empty() &&
           !m_protocolVersion.empty() &&
           m_chainStatus.isValid() &&
           m_networkId == m_chainStatus.networkId() &&
           m_chainId == m_chainStatus.chainId() &&
           m_protocolVersion == m_chainStatus.protocolVersion() &&
           m_createdAt > 0;
}

std::string PeerHelloMessage::serialize() const {
    std::ostringstream output;
    output << "PeerHelloMessage{"
           << "peer=" << m_peer.serialize()
           << ";networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";protocolVersion=" << m_protocolVersion
           << ";chainStatus=" << m_chainStatus.serialize()
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

PeerHandshakeResult::PeerHandshakeResult()
    : m_status(PeerHandshakeStatus::REJECTED),
      m_reason("Uninitialized peer handshake result.") {}

PeerHandshakeResult::PeerHandshakeResult(PeerHandshakeStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

PeerHandshakeStatus PeerHandshakeResult::status() const {
    return m_status;
}

const std::string& PeerHandshakeResult::reason() const {
    return m_reason;
}

bool PeerHandshakeResult::accepted() const {
    return m_status == PeerHandshakeStatus::ACCEPTED;
}

std::string PeerHandshakeResult::serialize() const {
    std::ostringstream output;
    output << "PeerHandshakeResult{status="
           << (accepted() ? "ACCEPTED" : "REJECTED")
           << ";reason=" << m_reason
           << "}";
    return output.str();
}

NetworkEnvelope PeerHandshakeManager::createHelloEnvelope(
    const GossipMeshConfig& config,
    const PeerMetadata& localPeer,
    const node::ChainStatusMessage& chainStatus,
    std::int64_t now
) {
    const PeerHelloMessage hello(
        localPeer,
        config.networkId(),
        config.chainId(),
        config.protocolVersion(),
        chainStatus,
        now
    );

    return NetworkEnvelope(
        config.networkId(),
        config.chainId(),
        config.protocolVersion(),
        NetworkMessageType::PEER_HELLO,
        config.localNodeId(),
        now,
        config.defaultTtlSeconds(),
        hello.serialize()
    );
}

PeerHandshakeResult PeerHandshakeManager::validateHello(
    const GossipMeshConfig& config,
    const NetworkEnvelope& envelope,
    std::int64_t now
) {
    if (!config.isValid()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Local gossip mesh config is invalid."
        );
    }

    if (!envelope.isStructurallyValid(1024 * 1024)) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello envelope is structurally invalid."
        );
    }

    if (envelope.messageType() != NetworkMessageType::PEER_HELLO) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello envelope has wrong message type."
        );
    }

    if (envelope.networkId() != config.networkId() ||
        envelope.chainId() != config.chainId() ||
        envelope.protocolVersion() != config.protocolVersion()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello network, chain, or protocol does not match."
        );
    }

    if (envelope.senderNodeId() == config.localNodeId()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello came from the local node id."
        );
    }

    if (envelope.expiredAt(now)) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello envelope is expired."
        );
    }

    return PeerHandshakeResult(
        PeerHandshakeStatus::ACCEPTED,
        "Peer hello accepted."
    );
}

} // namespace nodo::p2p
