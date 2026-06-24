#ifndef NODO_P2P_NETWORK_ENVELOPE_HPP
#define NODO_P2P_NETWORK_ENVELOPE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::p2p {

enum class NetworkMessageType {
    UNKNOWN,
    PING,
    PONG,
    PEER_HELLO,
    PEER_STATUS,
    TRANSACTION_ANNOUNCE,
    TRANSACTION_REQUEST,
    TRANSACTION_RESPONSE,
    TRANSACTION_GOSSIP,
    BLOCK_ANNOUNCE,
    BLOCK_REQUEST,
    BLOCK_RESPONSE,
    BLOCK_PROPOSAL,
    VOTE_ANNOUNCE,
    VALIDATOR_VOTE,
    QUORUM_CERTIFICATE_ANNOUNCE,
    QUORUM_CERTIFICATE,
    FINALIZED_BLOCK_ARTIFACT,
    CHAIN_STATUS,
    BLOCK_SYNC_REQUEST,
    BLOCK_SYNC_RESPONSE
};

std::string networkMessageTypeToString(NetworkMessageType type);
NetworkMessageType networkMessageTypeFromString(const std::string& value);

class NetworkEnvelope {
public:
    NetworkEnvelope();

    NetworkEnvelope(
        std::string networkId,
        std::string chainId,
        std::string protocolVersion,
        NetworkMessageType messageType,
        std::string senderNodeId,
        std::int64_t createdAt,
        std::uint32_t ttlSeconds,
        std::string payload
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    NetworkMessageType messageType() const;
    const std::string& messageId() const;
    const std::string& senderNodeId() const;
    std::int64_t createdAt() const;
    std::uint32_t ttlSeconds() const;
    const std::string& payload() const;
    const std::string& payloadHash() const;

    bool expiredAt(std::int64_t now) const;
    bool payloadHashMatches() const;
    bool isStructurallyValid(std::size_t maxPayloadBytes) const;

    std::string signingPayload() const;
    std::string serialize() const;

    static std::string hashPayload(const std::string& payload);
    static std::string computeMessageId(const NetworkEnvelope& envelope);

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    NetworkMessageType m_messageType;
    std::string m_messageId;
    std::string m_senderNodeId;
    std::int64_t m_createdAt;
    std::uint32_t m_ttlSeconds;
    std::string m_payload;
    std::string m_payloadHash;
};

} // namespace nodo::p2p

#endif
