#include "p2p/NetworkEnvelope.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

bool isSafeScalar(const std::string& value) {
    if (value.empty() || value.size() > 160) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string hashString(const std::string& value) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

} // namespace

std::string networkMessageTypeToString(NetworkMessageType type) {
    switch (type) {
        case NetworkMessageType::PING:                        return "PING";
        case NetworkMessageType::PONG:                        return "PONG";
        case NetworkMessageType::PEER_CHALLENGE:              return "PEER_CHALLENGE";
        case NetworkMessageType::PEER_HELLO:                  return "PEER_HELLO";
        case NetworkMessageType::PEER_STATUS:                 return "PEER_STATUS";
        case NetworkMessageType::PEER_EXCHANGE:               return "PEER_EXCHANGE";
        case NetworkMessageType::TRANSACTION_ANNOUNCE:        return "TRANSACTION_ANNOUNCE";
        case NetworkMessageType::TRANSACTION_REQUEST:         return "TRANSACTION_REQUEST";
        case NetworkMessageType::TRANSACTION_RESPONSE:        return "TRANSACTION_RESPONSE";
        case NetworkMessageType::TRANSACTION_GOSSIP:          return "TRANSACTION_GOSSIP";
        case NetworkMessageType::BLOCK_ANNOUNCE:              return "BLOCK_ANNOUNCE";
        case NetworkMessageType::BLOCK_REQUEST:               return "BLOCK_REQUEST";
        case NetworkMessageType::BLOCK_RESPONSE:              return "BLOCK_RESPONSE";
        case NetworkMessageType::BLOCK_PROPOSAL:              return "BLOCK_PROPOSAL";
        case NetworkMessageType::VOTE_ANNOUNCE:               return "VOTE_ANNOUNCE";
        case NetworkMessageType::VALIDATOR_VOTE:              return "VALIDATOR_VOTE";
        case NetworkMessageType::QUORUM_CERTIFICATE_ANNOUNCE: return "QUORUM_CERTIFICATE_ANNOUNCE";
        case NetworkMessageType::QUORUM_CERTIFICATE:          return "QUORUM_CERTIFICATE";
        case NetworkMessageType::FINALIZED_BLOCK_ARTIFACT:    return "FINALIZED_BLOCK_ARTIFACT";
        case NetworkMessageType::CHAIN_STATUS:                return "CHAIN_STATUS";
        case NetworkMessageType::BLOCK_SYNC_REQUEST:          return "BLOCK_SYNC_REQUEST";
        case NetworkMessageType::BLOCK_SYNC_RESPONSE:         return "BLOCK_SYNC_RESPONSE";
        case NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE:  return "SLASHING_EVIDENCE_ANNOUNCE";
        case NetworkMessageType::SLASHING_EVIDENCE_INVENTORY: return "SLASHING_EVIDENCE_INVENTORY";
        case NetworkMessageType::SLASHING_EVIDENCE_REQUEST:   return "SLASHING_EVIDENCE_REQUEST";
        case NetworkMessageType::SLASHING_EVIDENCE_RESPONSE:  return "SLASHING_EVIDENCE_RESPONSE";
        case NetworkMessageType::UNKNOWN:
        default: return "UNKNOWN";
    }
}

NetworkMessageType networkMessageTypeFromString(const std::string& value) {
    if (value == "PING")                        return NetworkMessageType::PING;
    if (value == "PONG")                        return NetworkMessageType::PONG;
    if (value == "PEER_CHALLENGE")              return NetworkMessageType::PEER_CHALLENGE;
    if (value == "PEER_HELLO")                  return NetworkMessageType::PEER_HELLO;
    if (value == "PEER_STATUS")                 return NetworkMessageType::PEER_STATUS;
    if (value == "PEER_EXCHANGE")               return NetworkMessageType::PEER_EXCHANGE;
    if (value == "TRANSACTION_ANNOUNCE")        return NetworkMessageType::TRANSACTION_ANNOUNCE;
    if (value == "TRANSACTION_REQUEST")         return NetworkMessageType::TRANSACTION_REQUEST;
    if (value == "TRANSACTION_RESPONSE")        return NetworkMessageType::TRANSACTION_RESPONSE;
    if (value == "TRANSACTION_GOSSIP")          return NetworkMessageType::TRANSACTION_GOSSIP;
    if (value == "BLOCK_ANNOUNCE")              return NetworkMessageType::BLOCK_ANNOUNCE;
    if (value == "BLOCK_REQUEST")               return NetworkMessageType::BLOCK_REQUEST;
    if (value == "BLOCK_RESPONSE")              return NetworkMessageType::BLOCK_RESPONSE;
    if (value == "BLOCK_PROPOSAL")              return NetworkMessageType::BLOCK_PROPOSAL;
    if (value == "VOTE_ANNOUNCE")               return NetworkMessageType::VOTE_ANNOUNCE;
    if (value == "VALIDATOR_VOTE")              return NetworkMessageType::VALIDATOR_VOTE;
    if (value == "QUORUM_CERTIFICATE_ANNOUNCE") return NetworkMessageType::QUORUM_CERTIFICATE_ANNOUNCE;
    if (value == "QUORUM_CERTIFICATE")          return NetworkMessageType::QUORUM_CERTIFICATE;
    if (value == "FINALIZED_BLOCK_ARTIFACT")    return NetworkMessageType::FINALIZED_BLOCK_ARTIFACT;
    if (value == "CHAIN_STATUS")                return NetworkMessageType::CHAIN_STATUS;
    if (value == "BLOCK_SYNC_REQUEST")          return NetworkMessageType::BLOCK_SYNC_REQUEST;
    if (value == "BLOCK_SYNC_RESPONSE")         return NetworkMessageType::BLOCK_SYNC_RESPONSE;
    if (value == "SLASHING_EVIDENCE_ANNOUNCE")  return NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE;
    if (value == "SLASHING_EVIDENCE_INVENTORY") return NetworkMessageType::SLASHING_EVIDENCE_INVENTORY;
    if (value == "SLASHING_EVIDENCE_REQUEST")   return NetworkMessageType::SLASHING_EVIDENCE_REQUEST;
    if (value == "SLASHING_EVIDENCE_RESPONSE")  return NetworkMessageType::SLASHING_EVIDENCE_RESPONSE;
    return NetworkMessageType::UNKNOWN;
}

NetworkEnvelope::NetworkEnvelope()
    : m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_messageType(NetworkMessageType::UNKNOWN),
      m_messageId(""),
      m_senderNodeId(""),
      m_createdAt(0),
      m_ttlSeconds(0),
      m_payload(""),
      m_payloadHash("") {}

NetworkEnvelope::NetworkEnvelope(
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    NetworkMessageType messageType,
    std::string senderNodeId,
    std::int64_t createdAt,
    std::uint32_t ttlSeconds,
    std::string payload
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_messageType(messageType),
    m_messageId(""),
    m_senderNodeId(std::move(senderNodeId)),
    m_createdAt(createdAt),
    m_ttlSeconds(ttlSeconds),
    m_payload(std::move(payload)),
    m_payloadHash(hashPayload(m_payload)) {
    m_messageId = computeMessageId(*this);
}

const std::string& NetworkEnvelope::networkId() const { return m_networkId; }
const std::string& NetworkEnvelope::chainId() const { return m_chainId; }
const std::string& NetworkEnvelope::protocolVersion() const { return m_protocolVersion; }
NetworkMessageType NetworkEnvelope::messageType() const { return m_messageType; }
const std::string& NetworkEnvelope::messageId() const { return m_messageId; }
const std::string& NetworkEnvelope::senderNodeId() const { return m_senderNodeId; }
std::int64_t NetworkEnvelope::createdAt() const { return m_createdAt; }
std::uint32_t NetworkEnvelope::ttlSeconds() const { return m_ttlSeconds; }
const std::string& NetworkEnvelope::payload() const { return m_payload; }
const std::string& NetworkEnvelope::payloadHash() const { return m_payloadHash; }

bool NetworkEnvelope::expiredAt(std::int64_t now) const {
    if (now <= 0 || m_createdAt <= 0) {
        return true;
    }

    return now > m_createdAt &&
           static_cast<std::uint64_t>(now - m_createdAt) > m_ttlSeconds;
}

bool NetworkEnvelope::payloadHashMatches() const {
    return m_payloadHash == hashPayload(m_payload);
}

bool NetworkEnvelope::isStructurallyValid(std::size_t maxPayloadBytes) const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
           isSafeScalar(m_senderNodeId) &&
           m_messageType != NetworkMessageType::UNKNOWN &&
           m_messageId == computeMessageId(*this) &&
           m_createdAt > 0 &&
           m_ttlSeconds > 0 &&
           m_payload.size() <= maxPayloadBytes &&
           payloadHashMatches();
}

std::string NetworkEnvelope::signingPayload() const {
    std::ostringstream output;
    output << "NodoNetworkEnvelopeSigningPayload{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";protocolVersion=" << m_protocolVersion
           << ";messageType=" << networkMessageTypeToString(m_messageType)
           << ";senderNodeId=" << m_senderNodeId
           << ";createdAt=" << m_createdAt
           << ";ttlSeconds=" << m_ttlSeconds
           << ";payloadHash=" << m_payloadHash
           << "}";
    return output.str();
}

std::string NetworkEnvelope::serialize() const {
    std::ostringstream output;
    output << "NetworkEnvelope{"
           << "messageId=" << m_messageId
           << ";networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";protocolVersion=" << m_protocolVersion
           << ";messageType=" << networkMessageTypeToString(m_messageType)
           << ";senderNodeId=" << m_senderNodeId
           << ";createdAt=" << m_createdAt
           << ";ttlSeconds=" << m_ttlSeconds
           << ";payloadHash=" << m_payloadHash
           << ";payload=" << m_payload
           << "}";
    return output.str();
}

std::string NetworkEnvelope::hashPayload(const std::string& payload) {
    return hashString("NODO_NETWORK_PAYLOAD_V1|" + payload);
}

std::string NetworkEnvelope::computeMessageId(const NetworkEnvelope& envelope) {
    return hashString(envelope.signingPayload());
}

} // namespace nodo::p2p
