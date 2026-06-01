#ifndef NODO_P2P_ENCRYPTED_PEER_HANDSHAKE_HPP
#define NODO_P2P_ENCRYPTED_PEER_HANDSHAKE_HPP

#include "p2p/EncryptedPeerChannel.hpp"

#include <cstdint>
#include <string>

namespace nodo::p2p {

class EncryptedPeerHandshakeHello {
public:
    EncryptedPeerHandshakeHello();

    EncryptedPeerHandshakeHello(
        std::string nodeId,
        std::string publicKeyFingerprint,
        std::string nonceHex,
        std::int64_t createdAt
    );

    const std::string& nodeId() const;
    const std::string& publicKeyFingerprint() const;
    const std::string& nonceHex() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string challengePayload() const;
    std::string serialize() const;

private:
    std::string m_nodeId;
    std::string m_publicKeyFingerprint;
    std::string m_nonceHex;
    std::int64_t m_createdAt;
};

class EncryptedPeerHandshakeAccept {
public:
    EncryptedPeerHandshakeAccept();

    EncryptedPeerHandshakeAccept(
        std::string responderNodeId,
        std::string responderFingerprint,
        std::string initiatorNonceHex,
        std::string responderNonceHex,
        std::string sessionId,
        std::string authenticationProofHex,
        std::int64_t createdAt
    );

    const std::string& responderNodeId() const;
    const std::string& responderFingerprint() const;
    const std::string& initiatorNonceHex() const;
    const std::string& responderNonceHex() const;
    const std::string& sessionId() const;
    const std::string& authenticationProofHex() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string proofPayload() const;
    std::string serialize() const;

private:
    std::string m_responderNodeId;
    std::string m_responderFingerprint;
    std::string m_initiatorNonceHex;
    std::string m_responderNonceHex;
    std::string m_sessionId;
    std::string m_authenticationProofHex;
    std::int64_t m_createdAt;
};

enum class EncryptedPeerHandshakeStatus {
    ACCEPTED,
    REJECTED
};

class EncryptedPeerHandshakeResult {
public:
    EncryptedPeerHandshakeResult();

    EncryptedPeerHandshakeResult(
        EncryptedPeerHandshakeStatus status,
        std::string reason,
        std::string sessionId
    );

    EncryptedPeerHandshakeStatus status() const;
    const std::string& reason() const;
    const std::string& sessionId() const;
    bool accepted() const;
    std::string serialize() const;

private:
    EncryptedPeerHandshakeStatus m_status;
    std::string m_reason;
    std::string m_sessionId;
};

class EncryptedPeerHandshakeManager {
public:
    static EncryptedPeerHandshakeHello createHello(
        const std::string& nodeId,
        const std::string& publicKeyFingerprint,
        std::int64_t now
    );

    static EncryptedPeerHandshakeAccept createAccept(
        const EncryptedPeerHandshakeHello& hello,
        const std::string& responderNodeId,
        const std::string& responderFingerprint,
        const std::string& sharedSecret,
        std::int64_t now
    );

    static EncryptedPeerHandshakeResult validateAccept(
        const EncryptedPeerHandshakeHello& hello,
        const EncryptedPeerHandshakeAccept& accept,
        const std::string& expectedResponderNodeId,
        const std::string& expectedResponderFingerprint,
        const std::string& sharedSecret
    );
};

} // namespace nodo::p2p

#endif
