#include "p2p/PeerHandshakeManager.hpp"

#include "core/ProtocolLimits.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
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
    const std::string marker = fieldName + "=";
    const std::size_t openingBrace = payload.find('{');
    if (openingBrace == std::string::npos) {
        return "";
    }
    std::size_t depth = 0;
    for (std::size_t position = openingBrace + 1;
         position < payload.size();
         ++position) {
        const char character = payload[position];
        if (character == '{' || character == '[') {
            ++depth;
            continue;
        }
        if (character == '}' || character == ']') {
            if (depth > 0) --depth;
            continue;
        }
        if (character != ';' || depth != 0 ||
            payload.compare(position + 1, marker.size(), marker) != 0) {
            continue;
        }

        const std::size_t valueStart = position + 1 + marker.size();
        std::size_t valueEnd = valueStart;
        std::size_t valueDepth = 0;
        while (valueEnd < payload.size()) {
            const char valueCharacter = payload[valueEnd];
            if (valueCharacter == '{' || valueCharacter == '[') {
                ++valueDepth;
            } else if (valueCharacter == '}' || valueCharacter == ']') {
                if (valueDepth == 0) break;
                --valueDepth;
            } else if (valueCharacter == ';' && valueDepth == 0) {
                break;
            }
            ++valueEnd;
        }
        return payload.substr(valueStart, valueEnd - valueStart);
    }
    return "";
}

std::string extractNestedField(
    const std::string& payload,
    const std::string& fieldName
) {
    const std::string marker = fieldName + "=";
    std::size_t markerPosition = payload.find(marker);
    while (markerPosition != std::string::npos) {
        if (markerPosition == 0 || payload[markerPosition - 1] == '{' ||
            payload[markerPosition - 1] == ';') {
            break;
        }
        markerPosition = payload.find(marker, markerPosition + 1);
    }
    if (markerPosition == std::string::npos) {
        return "";
    }

    const std::size_t valueStart = markerPosition + marker.size();
    std::size_t depth = 0;
    std::size_t position = valueStart;
    while (position < payload.size()) {
        const char character = payload[position];
        if (character == '{' || character == '[') {
            ++depth;
        } else if (character == '}' || character == ']') {
            if (depth == 0) break;
            --depth;
        } else if (character == ';' && depth == 0) {
            break;
        }
        ++position;
    }
    return payload.substr(valueStart, position - valueStart);
}

bool parseUnsigned16(const std::string& value, std::uint16_t& parsed) {
    if (value.empty()) return false;
    for (const char character : value) {
        if (character < '0' || character > '9') return false;
    }
    try {
        std::size_t consumed = 0;
        const unsigned long long number = std::stoull(value, &consumed);
        if (consumed != value.size() || number == 0 ||
            number > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        parsed = static_cast<std::uint16_t>(number);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseInt64(const std::string& value, std::int64_t& parsed) {
    if (value.empty()) return false;
    try {
        std::size_t consumed = 0;
        const long long number = std::stoll(value, &consumed);
        if (consumed != value.size()) return false;
        parsed = static_cast<std::int64_t>(number);
        return true;
    } catch (...) {
        return false;
    }
}

// Returns true if the payload string looks structurally like a PeerHelloMessage.
bool hasHelloMessageWrapper(const std::string& payload) {
    return payload.rfind("PeerHelloMessage{", 0) == 0 &&
           payload.back() == '}';
}

std::string buildIdentityProofPayload(
    const std::string& peer,
    const std::string& networkId,
    const std::string& chainId,
    const std::string& protocolVersion,
    const std::string& genesisId,
    const std::string& chainStatus,
    std::int64_t createdAt
) {
    std::ostringstream output;
    output << "PeerHelloIdentityProofPayload{"
           << "peer=" << peer
           << ";networkId=" << networkId
           << ";chainId=" << chainId
           << ";protocolVersion=" << protocolVersion
           << ";genesisId=" << genesisId
           << ";chainStatus=" << chainStatus
           << ";createdAt=" << createdAt
           << "}";
    return output.str();
}

} // namespace

PeerHelloMessage::PeerHelloMessage()
    : m_peer(),
      m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_genesisId(""),
      m_chainStatus(),
      m_identityProof(),
      m_createdAt(0) {}

PeerHelloMessage::PeerHelloMessage(
    PeerMetadata peer,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::string genesisId,
    node::ChainStatusMessage chainStatus,
    crypto::SignatureBundle identityProof,
    std::int64_t createdAt
) : m_peer(std::move(peer)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_genesisId(std::move(genesisId)),
    m_chainStatus(std::move(chainStatus)),
    m_identityProof(std::move(identityProof)),
    m_createdAt(createdAt) {}

const PeerMetadata& PeerHelloMessage::peer() const { return m_peer; }
const std::string& PeerHelloMessage::networkId() const { return m_networkId; }
const std::string& PeerHelloMessage::chainId() const { return m_chainId; }
const std::string& PeerHelloMessage::protocolVersion() const { return m_protocolVersion; }
const std::string& PeerHelloMessage::genesisId() const { return m_genesisId; }
const node::ChainStatusMessage& PeerHelloMessage::chainStatus() const { return m_chainStatus; }
const crypto::SignatureBundle& PeerHelloMessage::identityProof() const { return m_identityProof; }
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
           !m_identityProof.empty() &&
           m_createdAt > 0;
}

std::string PeerHelloMessage::signingPayload() const {
    return buildIdentityProofPayload(
        m_peer.serialize(),
        m_networkId,
        m_chainId,
        m_protocolVersion,
        m_genesisId,
        m_chainStatus.serialize(),
        m_createdAt
    );
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
           << ";identityProof=" << m_identityProof.serialize()
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
    const crypto::KeyPair& nodeIdentityKey,
    std::int64_t now
) {
    if (!config.isValid() || !localPeer.isValid() ||
        !chainStatus.isValid() || !nodeIdentityKey.isValid() || now <= 0 ||
        nodeIdentityKey.algorithm() !=
            crypto::CryptoAlgorithm::CLASSIC_ED25519 ||
        localPeer.nodeId() != config.localNodeId() ||
        chainStatus.networkId() != config.networkId() ||
        chainStatus.chainId() != config.chainId() ||
        chainStatus.protocolVersion() != config.protocolVersion() ||
        localPeer.publicKeyFingerprint() !=
            nodeIdentityKey.publicKey().fingerprint()) {
        throw std::invalid_argument(
            "Cannot create PEER_HELLO from mismatched node identity."
        );
    }

    const PeerMetadata handshakePeer(
        localPeer.nodeId(),
        localPeer.endpoint(),
        localPeer.publicKeyFingerprint(),
        localPeer.firstSeenAt(),
        localPeer.lastSeenAt(),
        0,
        false
    );
    const PeerHelloMessage unsignedHello(
        handshakePeer,
        config.networkId(),
        config.chainId(),
        config.protocolVersion(),
        config.genesisId(),
        chainStatus,
        crypto::SignatureBundle(),
        now
    );
    const crypto::Ed25519SignatureProvider provider;
    const crypto::SignatureBundle identityProof = nodeIdentityKey.sign(
        unsignedHello.signingPayload(),
        now,
        provider,
        crypto::SigningDomain::PEER_HANDSHAKE
    );
    const PeerHelloMessage hello(
        handshakePeer,
        config.networkId(),
        config.chainId(),
        config.protocolVersion(),
        config.genesisId(),
        chainStatus,
        identityProof,
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

    if (!envelope.isStructurallyValid(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES)) {
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

        if (extractSerializedField(payload, "networkId") !=
                envelope.networkId() ||
            extractSerializedField(payload, "chainId") !=
                envelope.chainId() ||
            extractSerializedField(payload, "protocolVersion") !=
                envelope.protocolVersion()) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello payload identity does not match its envelope."
            );
        }

        std::int64_t payloadCreatedAt = 0;
        if (!parseInt64(
                extractSerializedField(payload, "createdAt"),
                payloadCreatedAt) ||
            payloadCreatedAt != envelope.createdAt()) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello timestamp does not match its envelope."
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

    const std::optional<PeerMetadata> peer =
        peerMetadataFromHello(envelope, now);
    if (!peer.has_value()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello contains invalid peer identity metadata."
        );
    }
    if (peer->nodeId() != envelope.senderNodeId()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello node id does not match envelope sender identity."
        );
    }


    const std::string peerBlock =
        extractNestedField(envelope.payload(), "peer");
    if (peerBlock != peer->serialize()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello identity metadata is not canonical."
        );
    }

    const std::string chainStatusBlock =
        extractSerializedField(envelope.payload(), "chainStatus");
    if (chainStatusBlock.rfind("ChainStatusMessage{", 0) != 0) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello chain status is malformed."
        );
    }

    try {
        const crypto::SignatureBundle proof =
            crypto::SignatureBundle::deserialize(
                extractSerializedField(envelope.payload(), "identityProof")
            );
        if (proof.signatures().size() != 1) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello must contain exactly one identity signature."
            );
        }

        const crypto::Signature& signature = proof.signatures().front();
        if (signature.algorithm() !=
                crypto::CryptoAlgorithm::CLASSIC_ED25519 ||
            signature.domain() != crypto::SigningDomain::PEER_HANDSHAKE ||
            signature.createdAt() != envelope.createdAt() ||
            signature.publicKey().fingerprint() !=
                peer->publicKeyFingerprint()) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello identity signature does not match its fingerprint."
            );
        }

        const std::string proofPayload = buildIdentityProofPayload(
            peerBlock,
            extractSerializedField(envelope.payload(), "networkId"),
            extractSerializedField(envelope.payload(), "chainId"),
            extractSerializedField(envelope.payload(), "protocolVersion"),
            extractSerializedField(envelope.payload(), "genesisId"),
            chainStatusBlock,
            envelope.createdAt()
        );
        const crypto::Ed25519SignatureProvider provider;
        if (!proof.verifyForPolicy(
                proofPayload,
                crypto::CryptoPolicy::developmentPolicy(),
                crypto::SecurityContext::PEER_AUTHENTICATION,
                provider)) {
            return PeerHandshakeResult(
                PeerHandshakeStatus::REJECTED,
                "Peer hello identity proof verification failed."
            );
        }
    } catch (const std::exception&) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello identity proof is malformed."
        );
    }

    return PeerHandshakeResult(
        PeerHandshakeStatus::ACCEPTED,
        "Peer hello accepted."
    );
}

std::optional<PeerMetadata> PeerHandshakeManager::peerMetadataFromHello(
    const NetworkEnvelope& envelope,
    std::int64_t now
) {
    if (now <= 0 || !hasHelloMessageWrapper(envelope.payload())) {
        return std::nullopt;
    }

    const std::string peerBlock =
        extractNestedField(envelope.payload(), "peer");
    if (peerBlock.rfind("PeerMetadata{", 0) != 0) {
        return std::nullopt;
    }

    const std::string endpointBlock =
        extractNestedField(peerBlock, "endpoint");
    if (endpointBlock.rfind("PeerEndpoint{", 0) != 0) {
        return std::nullopt;
    }

    std::uint16_t port = 0;
    if (!parseUnsigned16(
            extractNestedField(endpointBlock, "port"),
            port)) {
        return std::nullopt;
    }

    std::int64_t firstSeenAt = 0;
    std::int64_t lastSeenAt = 0;
    if (!parseInt64(
            extractNestedField(peerBlock, "firstSeenAt"),
            firstSeenAt) ||
        !parseInt64(
            extractNestedField(peerBlock, "lastSeenAt"),
            lastSeenAt)) {
        return std::nullopt;
    }

    constexpr std::int64_t kTimestampToleranceSeconds = 300;
    const std::int64_t maximumTimestamp =
        now > std::numeric_limits<std::int64_t>::max() -
                kTimestampToleranceSeconds
            ? std::numeric_limits<std::int64_t>::max()
            : now + kTimestampToleranceSeconds;
    if (firstSeenAt <= 0 || lastSeenAt < firstSeenAt ||
        firstSeenAt > maximumTimestamp || lastSeenAt > maximumTimestamp) {
        return std::nullopt;
    }

    PeerMetadata peer(
        extractNestedField(peerBlock, "nodeId"),
        PeerEndpoint(
            extractNestedField(endpointBlock, "host"),
            port
        ),
        extractNestedField(peerBlock, "publicKeyFingerprint"),
        firstSeenAt,
        lastSeenAt,
        0,
        false
    );
    if (!peer.isValid()) {
        return std::nullopt;
    }
    return peer;
}

} // namespace nodo::p2p
