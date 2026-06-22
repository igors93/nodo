#include "p2p/PeerHandshakeManager.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

// Extracts the value of a named field from a serialized key=value payload
// produced by PeerHelloMessage::serialize(). Fields are delimited by ';'.
// Nested brace groups (e.g., peer={...}) are skipped over so that a ';'
// inside a nested block does not terminate the value prematurely.
//
// Returns the extracted value, or an empty string if the field is absent.
std::string extractSerializedField(
    const std::string& payload,
    const std::string& fieldName
) {
    const std::string prefix = ";" + fieldName + "=";
    const auto startPos = payload.find(prefix);
    if (startPos == std::string::npos) {
        return "";
    }
    const auto valueStart = startPos + prefix.size();
    std::size_t depth = 0;
    std::size_t pos = valueStart;
    while (pos < payload.size()) {
        const char c = payload[pos];
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            if (depth == 0) break;
            --depth;
        } else if (c == ';' && depth == 0) {
            break;
        }
        ++pos;
    }
    return payload.substr(valueStart, pos - valueStart);
}

// Returns true if the payload string looks structurally like a PeerHelloMessage.
bool hasHelloMessageWrapper(const std::string& payload) {
    return payload.rfind("PeerHelloMessage{", 0) == 0 &&
           payload.back() == '}';
}

} // namespace

PeerHelloMessage::PeerHelloMessage()
    : m_peer(),
      m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_genesisId(""),
      m_chainStatus(),
      m_createdAt(0) {}

PeerHelloMessage::PeerHelloMessage(
    PeerMetadata peer,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::string genesisId,
    node::ChainStatusMessage chainStatus,
    std::int64_t createdAt
) : m_peer(std::move(peer)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_genesisId(std::move(genesisId)),
    m_chainStatus(std::move(chainStatus)),
    m_createdAt(createdAt) {}

const PeerMetadata& PeerHelloMessage::peer() const { return m_peer; }
const std::string& PeerHelloMessage::networkId() const { return m_networkId; }
const std::string& PeerHelloMessage::chainId() const { return m_chainId; }
const std::string& PeerHelloMessage::protocolVersion() const { return m_protocolVersion; }
const std::string& PeerHelloMessage::genesisId() const { return m_genesisId; }
const node::ChainStatusMessage& PeerHelloMessage::chainStatus() const { return m_chainStatus; }
std::int64_t PeerHelloMessage::createdAt() const { return m_createdAt; }

bool PeerHelloMessage::isValid() const {
    return m_peer.isValid() &&
           !m_networkId.empty() &&
           !m_chainId.empty() &&
           !m_protocolVersion.empty() &&
           !m_genesisId.empty() &&
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
           << ";genesisId=" << m_genesisId
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
        config.genesisId(),
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
            "Peer hello network, chain, or protocol does not match local node: "
            "expected network=" + config.networkId() +
            " chain=" + config.chainId() +
            " protocol=" + config.protocolVersion() + "."
        );
    }

    // Parse and validate genesis identity. The payload must be a well-formed
    // PeerHelloMessage serialization so genesis id is extracted as a discrete
    // field, not matched as a substring of arbitrary payload content.
    {
        const std::string& payload = envelope.payload();

        if (!hasHelloMessageWrapper(payload)) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello payload is not a valid PeerHelloMessage structure."
            );
        }

        const std::string peerGenesisId = extractSerializedField(payload, "genesisId");

        if (peerGenesisId.empty()) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello payload is missing the genesisId field."
            );
        }

        // config.isValid() (checked above) already guarantees config.genesisId()
        // is non-empty, so this comparison is always active. The previous guard
        // (!config.genesisId().empty()) made enforcement opt-in, allowing peers
        // from foreign chains to complete the handshake on mis-configured nodes.
        if (peerGenesisId != config.genesisId()) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello genesis id '" + peerGenesisId +
                "' does not match local genesis '" + config.genesisId() + "'."
            );
        }
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
