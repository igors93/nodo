#ifndef NODO_P2P_ENCRYPTED_PEER_CHANNEL_HPP
#define NODO_P2P_ENCRYPTED_PEER_CHANNEL_HPP

#include "p2p/NetworkEnvelope.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::p2p {

/*
 * Encrypted peer channels are the first authenticated confidentiality boundary
 * for testnet peer traffic.
 *
 * Important: this class deliberately lives above Transport and below GossipMesh.
 * Consensus code should never know whether the underlying delivery is loopback,
 * TCP, encrypted TCP, QUIC or something else.
 */
class EncryptedPeerChannelFrame {
public:
    EncryptedPeerChannelFrame();

    EncryptedPeerChannelFrame(
        std::string sessionId,
        std::string fromNodeId,
        std::string toNodeId,
        std::uint64_t sequence,
        std::int64_t createdAt,
        std::string nonceHex,
        std::string ciphertextHex,
        std::string authenticationTagHex
    );

    const std::string& sessionId() const;
    const std::string& fromNodeId() const;
    const std::string& toNodeId() const;
    std::uint64_t sequence() const;
    std::int64_t createdAt() const;
    const std::string& nonceHex() const;
    const std::string& ciphertextHex() const;
    const std::string& authenticationTagHex() const;

    bool isValid() const;
    std::string aadPayload() const;
    std::string serialize() const;

private:
    std::string m_sessionId;
    std::string m_fromNodeId;
    std::string m_toNodeId;
    std::uint64_t m_sequence;
    std::int64_t m_createdAt;
    std::string m_nonceHex;
    std::string m_ciphertextHex;
    std::string m_authenticationTagHex;
};

enum class EncryptedPeerChannelStatus {
    OPENED,
    REJECTED,
    INVALID_FRAME,
    WRONG_SESSION,
    AUTHENTICATION_FAILED,
    REPLAY_DETECTED
};

std::string encryptedPeerChannelStatusToString(
    EncryptedPeerChannelStatus status
);

class EncryptedPeerOpenResult {
public:
    EncryptedPeerOpenResult();

    EncryptedPeerOpenResult(
        EncryptedPeerChannelStatus status,
        std::string reason,
        std::optional<NetworkEnvelope> envelope
    );

    EncryptedPeerChannelStatus status() const;
    const std::string& reason() const;
    bool opened() const;
    const std::optional<NetworkEnvelope>& envelope() const;

    std::string serialize() const;

private:
    EncryptedPeerChannelStatus m_status;
    std::string m_reason;
    std::optional<NetworkEnvelope> m_envelope;
};

class EncryptedPeerSession {
public:
    EncryptedPeerSession();

    EncryptedPeerSession(
        std::string localNodeId,
        std::string remoteNodeId,
        std::string sharedSecret,
        std::int64_t establishedAt
    );

    const std::string& localNodeId() const;
    const std::string& remoteNodeId() const;
    const std::string& sessionId() const;
    std::uint64_t nextOutboundSequence() const;
    std::uint64_t lastInboundSequence() const;
    std::int64_t establishedAt() const;

    bool isValid() const;

    EncryptedPeerChannelFrame sealEnvelope(
        const NetworkEnvelope& envelope,
        std::int64_t now
    );

    EncryptedPeerOpenResult openFrame(
        const EncryptedPeerChannelFrame& frame
    );

    std::string serialize() const;

    static std::string deriveSessionId(
        const std::string& leftNodeId,
        const std::string& rightNodeId,
        const std::string& sharedSecret
    );

private:
    std::string m_localNodeId;
    std::string m_remoteNodeId;
    std::string m_sharedSecret;
    std::string m_sessionId;
    std::uint64_t m_nextOutboundSequence;
    std::uint64_t m_lastInboundSequence;
    std::int64_t m_establishedAt;

    std::string deriveKeyMaterial() const;
};

class EncryptedPeerChannelCodec {
public:
    static constexpr std::size_t MAX_ENCRYPTED_FRAME_BYTES = 1024 * 1024;

    static std::vector<unsigned char> encodeFrame(
        const EncryptedPeerChannelFrame& frame
    );

    static EncryptedPeerChannelFrame decodeFrame(
        const std::vector<unsigned char>& bytes
    );

    static std::string encodeFrameToString(
        const EncryptedPeerChannelFrame& frame
    );

    static EncryptedPeerChannelFrame decodeFrameFromString(
        const std::string& bytes
    );

    static bool isValidFrameBytes(
        const std::vector<unsigned char>& bytes
    );
};

} // namespace nodo::p2p

#endif
