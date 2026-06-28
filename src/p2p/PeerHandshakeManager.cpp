#include "p2p/PeerHandshakeManager.hpp"

#include "core/ProtocolLimits.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "p2p/PeerHandshakeReplayGuard.hpp"
#include "p2p/PeerSessionKeyAgreement.hpp"

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

bool parseUnsigned32(const std::string& value, std::uint32_t& parsed) {
    if (value.empty()) return false;
    for (const char character : value) {
        if (character < '0' || character > '9') return false;
    }
    try {
        std::size_t consumed = 0;
        const unsigned long long number = std::stoull(value, &consumed);
        if (consumed != value.size() || number == 0 ||
            number > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        parsed = static_cast<std::uint32_t>(number);
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

bool isSafeNodeId(const std::string& value) {
    if (value.empty() || value.size() > 160) return false;
    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';
        if (!allowed) return false;
    }
    return true;
}

std::string buildIdentityProofPayload(
    const std::string& peer,
    const std::string& networkId,
    const std::string& chainId,
    const std::string& protocolVersion,
    const std::string& genesisId,
    const std::string& chainStatus,
    const std::string& challengeIssuerNodeId,
    const std::string& challengeNonce,
    const std::string& challengeEphemeralPublicKeyHex,
    const std::string& ephemeralPublicKeyHex,
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
           << ";challengeIssuerNodeId=" << challengeIssuerNodeId
           << ";challengeNonce=" << challengeNonce
           << ";challengeEphemeralPublicKey="
           << challengeEphemeralPublicKeyHex
           << ";ephemeralPublicKey=" << ephemeralPublicKeyHex
           << ";createdAt=" << createdAt
           << "}";
    return output.str();
}

} // namespace

PeerChallengeMessage::PeerChallengeMessage()
    : m_challengerNodeId(""),
      m_challengedNodeId(""),
      m_nonce(""),
      m_ephemeralPublicKeyHex(""),
      m_createdAt(0),
      m_ttlSeconds(0) {}

PeerChallengeMessage::PeerChallengeMessage(
    std::string challengerNodeId,
    std::string challengedNodeId,
    std::string nonce,
    std::string ephemeralPublicKeyHex,
    std::int64_t createdAt,
    std::uint32_t ttlSeconds
) : m_challengerNodeId(std::move(challengerNodeId)),
    m_challengedNodeId(std::move(challengedNodeId)),
    m_nonce(std::move(nonce)),
    m_ephemeralPublicKeyHex(std::move(ephemeralPublicKeyHex)),
    m_createdAt(createdAt),
    m_ttlSeconds(ttlSeconds) {}

const std::string& PeerChallengeMessage::challengerNodeId() const {
    return m_challengerNodeId;
}

const std::string& PeerChallengeMessage::challengedNodeId() const {
    return m_challengedNodeId;
}

const std::string& PeerChallengeMessage::nonce() const {
    return m_nonce;
}

const std::string& PeerChallengeMessage::ephemeralPublicKeyHex() const {
    return m_ephemeralPublicKeyHex;
}

std::int64_t PeerChallengeMessage::createdAt() const {
    return m_createdAt;
}

std::uint32_t PeerChallengeMessage::ttlSeconds() const {
    return m_ttlSeconds;
}

std::int64_t PeerChallengeMessage::expiresAt() const {
    if (!isValid()) return 0;
    return m_createdAt + static_cast<std::int64_t>(m_ttlSeconds);
}

bool PeerChallengeMessage::isValid() const {
    return isSafeNodeId(m_challengerNodeId) &&
           isSafeNodeId(m_challengedNodeId) &&
           m_challengerNodeId != m_challengedNodeId &&
           PeerHandshakeReplayGuard::isValidNonce(m_nonce) &&
           PeerSessionKeyAgreement::isValidPublicKey(
               m_ephemeralPublicKeyHex) &&
           m_createdAt > 0 &&
           m_ttlSeconds > 0 &&
           m_createdAt <= std::numeric_limits<std::int64_t>::max() -
               m_ttlSeconds;
}

std::string PeerChallengeMessage::serialize() const {
    std::ostringstream output;
    output << "PeerChallengeMessage{"
           << "challengerNodeId=" << m_challengerNodeId
           << ";challengedNodeId=" << m_challengedNodeId
           << ";nonce=" << m_nonce
           << ";ephemeralPublicKey=" << m_ephemeralPublicKeyHex
           << ";createdAt=" << m_createdAt
           << ";ttlSeconds=" << m_ttlSeconds
           << "}";
    return output.str();
}

PeerHelloMessage::PeerHelloMessage()
    : m_peer(),
      m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_genesisId(""),
      m_chainStatus(),
      m_challengeIssuerNodeId(""),
      m_challengeNonce(""),
      m_challengeEphemeralPublicKeyHex(""),
      m_ephemeralPublicKeyHex(""),
      m_identityProof(),
      m_createdAt(0) {}

PeerHelloMessage::PeerHelloMessage(
    PeerMetadata peer,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::string genesisId,
    node::ChainStatusMessage chainStatus,
    std::string challengeIssuerNodeId,
    std::string challengeNonce,
    std::string challengeEphemeralPublicKeyHex,
    std::string ephemeralPublicKeyHex,
    crypto::SignatureBundle identityProof,
    std::int64_t createdAt
) : m_peer(std::move(peer)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_genesisId(std::move(genesisId)),
    m_chainStatus(std::move(chainStatus)),
    m_challengeIssuerNodeId(std::move(challengeIssuerNodeId)),
    m_challengeNonce(std::move(challengeNonce)),
    m_challengeEphemeralPublicKeyHex(
        std::move(challengeEphemeralPublicKeyHex)),
    m_ephemeralPublicKeyHex(std::move(ephemeralPublicKeyHex)),
    m_identityProof(std::move(identityProof)),
    m_createdAt(createdAt) {}

const PeerMetadata& PeerHelloMessage::peer() const { return m_peer; }
const std::string& PeerHelloMessage::networkId() const { return m_networkId; }
const std::string& PeerHelloMessage::chainId() const { return m_chainId; }
const std::string& PeerHelloMessage::protocolVersion() const { return m_protocolVersion; }
const std::string& PeerHelloMessage::genesisId() const { return m_genesisId; }
const node::ChainStatusMessage& PeerHelloMessage::chainStatus() const { return m_chainStatus; }
const std::string& PeerHelloMessage::challengeIssuerNodeId() const {
    return m_challengeIssuerNodeId;
}
const std::string& PeerHelloMessage::challengeNonce() const { return m_challengeNonce; }
const std::string& PeerHelloMessage::challengeEphemeralPublicKeyHex() const {
    return m_challengeEphemeralPublicKeyHex;
}
const std::string& PeerHelloMessage::ephemeralPublicKeyHex() const {
    return m_ephemeralPublicKeyHex;
}
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
           isSafeNodeId(m_challengeIssuerNodeId) &&
           m_challengeIssuerNodeId != m_peer.nodeId() &&
           PeerHandshakeReplayGuard::isValidNonce(m_challengeNonce) &&
           PeerSessionKeyAgreement::isValidPublicKey(
               m_challengeEphemeralPublicKeyHex) &&
           PeerSessionKeyAgreement::isValidPublicKey(
               m_ephemeralPublicKeyHex) &&
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
        m_challengeIssuerNodeId,
        m_challengeNonce,
        m_challengeEphemeralPublicKeyHex,
        m_ephemeralPublicKeyHex,
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
           << ";challengeIssuerNodeId=" << m_challengeIssuerNodeId
           << ";challengeNonce=" << m_challengeNonce
           << ";challengeEphemeralPublicKey="
           << m_challengeEphemeralPublicKeyHex
           << ";ephemeralPublicKey=" << m_ephemeralPublicKeyHex
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

NetworkEnvelope PeerHandshakeManager::createChallengeEnvelope(
    const GossipMeshConfig& config,
    const std::string& challengedNodeId,
    const std::string& nonce,
    std::int64_t now
) {
    const auto keyPair = PeerSessionKeyAgreement::generateEphemeralKeyPair();
    if (!keyPair.has_value()) {
        throw std::runtime_error(
            "Could not generate peer handshake ephemeral key."
        );
    }
    return createChallengeEnvelope(
        config,
        challengedNodeId,
        nonce,
        keyPair->publicKeyHex,
        now
    );
}

NetworkEnvelope PeerHandshakeManager::createChallengeEnvelope(
    const GossipMeshConfig& config,
    const std::string& challengedNodeId,
    const std::string& nonce,
    const std::string& ephemeralPublicKeyHex,
    std::int64_t now
) {
    const PeerChallengeMessage challenge(
        config.localNodeId(),
        challengedNodeId,
        nonce,
        ephemeralPublicKeyHex,
        now,
        config.defaultTtlSeconds()
    );
    if (!config.isValid() || !challenge.isValid()) {
        throw std::invalid_argument(
            "Cannot create invalid peer handshake challenge."
        );
    }

    return NetworkEnvelope(
        config.networkId(),
        config.chainId(),
        config.protocolVersion(),
        NetworkMessageType::PEER_CHALLENGE,
        config.localNodeId(),
        now,
        config.defaultTtlSeconds(),
        challenge.serialize()
    );
}

std::optional<PeerChallengeMessage> PeerHandshakeManager::challengeFromEnvelope(
    const GossipMeshConfig& config,
    const NetworkEnvelope& envelope,
    std::int64_t now
) {
    if (!config.isValid() || now <= 0 ||
        !envelope.isStructurallyValid(
            core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES) ||
        envelope.messageType() != NetworkMessageType::PEER_CHALLENGE ||
        envelope.networkId() != config.networkId() ||
        envelope.chainId() != config.chainId() ||
        envelope.protocolVersion() != config.protocolVersion() ||
        envelope.senderNodeId() == config.localNodeId() ||
        envelope.expiredAt(now) ||
        envelope.payload().rfind("PeerChallengeMessage{", 0) != 0 ||
        envelope.payload().back() != '}') {
        return std::nullopt;
    }

    std::int64_t createdAt = 0;
    std::uint32_t ttlSeconds = 0;
    if (!parseInt64(
            extractSerializedField(envelope.payload(), "createdAt"),
            createdAt) ||
        !parseUnsigned32(
            extractSerializedField(envelope.payload(), "ttlSeconds"),
            ttlSeconds)) {
        return std::nullopt;
    }

    const PeerChallengeMessage challenge(
        extractNestedField(envelope.payload(), "challengerNodeId"),
        extractSerializedField(envelope.payload(), "challengedNodeId"),
        extractSerializedField(envelope.payload(), "nonce"),
        extractSerializedField(envelope.payload(), "ephemeralPublicKey"),
        createdAt,
        ttlSeconds
    );
    if (!challenge.isValid() || challenge.serialize() != envelope.payload() ||
        challenge.challengerNodeId() != envelope.senderNodeId() ||
        challenge.challengedNodeId() != config.localNodeId() ||
        challenge.createdAt() != envelope.createdAt() ||
        challenge.ttlSeconds() != envelope.ttlSeconds() ||
        now > challenge.expiresAt()) {
        return std::nullopt;
    }
    return challenge;
}

NetworkEnvelope PeerHandshakeManager::createHelloEnvelope(
    const GossipMeshConfig& config,
    const PeerMetadata& localPeer,
    const node::ChainStatusMessage& chainStatus,
    const std::string& challengeIssuerNodeId,
    const std::string& challengeNonce,
    const crypto::KeyPair& nodeIdentityKey,
    std::int64_t now
) {
    const auto challengeKeyPair =
        PeerSessionKeyAgreement::generateEphemeralKeyPair();
    const auto responseKeyPair =
        PeerSessionKeyAgreement::generateEphemeralKeyPair();
    if (!challengeKeyPair.has_value() || !responseKeyPair.has_value()) {
        throw std::runtime_error(
            "Could not generate peer handshake response key."
        );
    }
    return createHelloEnvelope(
        config,
        localPeer,
        chainStatus,
        challengeIssuerNodeId,
        challengeNonce,
        challengeKeyPair->publicKeyHex,
        responseKeyPair->publicKeyHex,
        nodeIdentityKey,
        now
    );
}

NetworkEnvelope PeerHandshakeManager::createHelloEnvelope(
    const GossipMeshConfig& config,
    const PeerMetadata& localPeer,
    const node::ChainStatusMessage& chainStatus,
    const std::string& challengeIssuerNodeId,
    const std::string& challengeNonce,
    const std::string& challengeEphemeralPublicKeyHex,
    const std::string& ephemeralPublicKeyHex,
    const crypto::KeyPair& nodeIdentityKey,
    std::int64_t now
) {
    if (!config.isValid() || !localPeer.isValid() ||
        !chainStatus.isValid() ||
        !isSafeNodeId(challengeIssuerNodeId) ||
        challengeIssuerNodeId == config.localNodeId() ||
        !PeerHandshakeReplayGuard::isValidNonce(challengeNonce) ||
        !PeerSessionKeyAgreement::isValidPublicKey(
            challengeEphemeralPublicKeyHex) ||
        !PeerSessionKeyAgreement::isValidPublicKey(ephemeralPublicKeyHex) ||
        !nodeIdentityKey.isValid() || now <= 0 ||
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
        challengeIssuerNodeId,
        challengeNonce,
        challengeEphemeralPublicKeyHex,
        ephemeralPublicKeyHex,
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
        challengeIssuerNodeId,
        challengeNonce,
        challengeEphemeralPublicKeyHex,
        ephemeralPublicKeyHex,
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
    const std::string& expectedChallengeNonce,
    std::int64_t now
) {
    return validateHello(
        config,
        envelope,
        expectedChallengeNonce,
        extractSerializedField(
            envelope.payload(), "challengeEphemeralPublicKey"),
        now
    );
}

PeerHandshakeResult PeerHandshakeManager::validateHello(
    const GossipMeshConfig& config,
    const NetworkEnvelope& envelope,
    const std::string& expectedChallengeNonce,
    const std::string& expectedChallengeEphemeralPublicKeyHex,
    std::int64_t now
) {
    if (!config.isValid()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Local gossip mesh config is invalid."
        );
    }

    if (!PeerHandshakeReplayGuard::isValidNonce(expectedChallengeNonce)) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Expected peer handshake challenge nonce is invalid."
        );
    }

    if (!PeerSessionKeyAgreement::isValidPublicKey(
            expectedChallengeEphemeralPublicKeyHex)) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Expected peer handshake session public key is invalid."
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

    const std::string challengeIssuerNodeId =
        extractSerializedField(
            envelope.payload(),
            "challengeIssuerNodeId"
        );
    if (challengeIssuerNodeId != config.localNodeId()) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello challenge issuer does not match the local node."
        );
    }

    const std::string challengeNonce =
        extractSerializedField(envelope.payload(), "challengeNonce");
    if (challengeNonce != expectedChallengeNonce) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello does not answer the outstanding challenge nonce."
        );
    }

    const std::string ephemeralPublicKeyHex =
        extractSerializedField(envelope.payload(), "ephemeralPublicKey");
    if (!PeerSessionKeyAgreement::isValidPublicKey(
            ephemeralPublicKeyHex)) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello session public key is malformed."
        );
    }

    const std::string challengeEphemeralPublicKeyHex =
        extractSerializedField(
            envelope.payload(), "challengeEphemeralPublicKey");
    if (challengeEphemeralPublicKeyHex !=
            expectedChallengeEphemeralPublicKeyHex) {
        return PeerHandshakeResult(
            PeerHandshakeStatus::REJECTED,
            "Peer hello does not bind the issued session public key."
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
            challengeIssuerNodeId,
            challengeNonce,
            challengeEphemeralPublicKeyHex,
            ephemeralPublicKeyHex,
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

std::string PeerHandshakeManager::challengeNonceFromHello(
    const NetworkEnvelope& envelope
) {
    if (envelope.messageType() != NetworkMessageType::PEER_HELLO ||
        !hasHelloMessageWrapper(envelope.payload())) {
        return "";
    }
    const std::string nonce =
        extractSerializedField(envelope.payload(), "challengeNonce");
    return PeerHandshakeReplayGuard::isValidNonce(nonce) ? nonce : "";
}

std::string PeerHandshakeManager::ephemeralPublicKeyFromHello(
    const NetworkEnvelope& envelope
) {
    if (envelope.messageType() != NetworkMessageType::PEER_HELLO ||
        !hasHelloMessageWrapper(envelope.payload())) {
        return "";
    }
    const std::string publicKey =
        extractSerializedField(envelope.payload(), "ephemeralPublicKey");
    return PeerSessionKeyAgreement::isValidPublicKey(publicKey)
        ? publicKey
        : "";
}

} // namespace nodo::p2p
