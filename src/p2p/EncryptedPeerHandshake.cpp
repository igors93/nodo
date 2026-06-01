#include "p2p/EncryptedPeerHandshake.hpp"

#include "serialization/CanonicalHash.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

constexpr const char* NONCE_DOMAIN = "NODO_ENCRYPTED_PEER_HANDSHAKE_NONCE_V1";
constexpr const char* PROOF_DOMAIN = "NODO_ENCRYPTED_PEER_HANDSHAKE_PROOF_V1";

bool isSafeScalar(const std::string& value, std::size_t maxSize = 256) {
    if (value.empty() || value.size() > maxSize) {
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

bool isHex(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        const bool hex =
            (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
        if (!hex) {
            return false;
        }
    }

    return true;
}

std::string hashText(
    const std::string& value,
    const std::string& domain
) {
    return serialization::CanonicalHash::hashString(value, domain);
}

std::string buildProof(
    const EncryptedPeerHandshakeHello& hello,
    const std::string& responderNodeId,
    const std::string& responderFingerprint,
    const std::string& responderNonceHex,
    const std::string& sessionId,
    const std::string& sharedSecret
) {
    return hashText(
        sharedSecret + "|" + hello.challengePayload() + "|" +
            responderNodeId + "|" + responderFingerprint + "|" +
            responderNonceHex + "|" + sessionId,
        PROOF_DOMAIN
    );
}

} // namespace

EncryptedPeerHandshakeHello::EncryptedPeerHandshakeHello()
    : m_nodeId(),
      m_publicKeyFingerprint(),
      m_nonceHex(),
      m_createdAt(0) {}

EncryptedPeerHandshakeHello::EncryptedPeerHandshakeHello(
    std::string nodeId,
    std::string publicKeyFingerprint,
    std::string nonceHex,
    std::int64_t createdAt
) : m_nodeId(std::move(nodeId)),
    m_publicKeyFingerprint(std::move(publicKeyFingerprint)),
    m_nonceHex(std::move(nonceHex)),
    m_createdAt(createdAt) {}

const std::string& EncryptedPeerHandshakeHello::nodeId() const { return m_nodeId; }
const std::string& EncryptedPeerHandshakeHello::publicKeyFingerprint() const { return m_publicKeyFingerprint; }
const std::string& EncryptedPeerHandshakeHello::nonceHex() const { return m_nonceHex; }
std::int64_t EncryptedPeerHandshakeHello::createdAt() const { return m_createdAt; }

bool EncryptedPeerHandshakeHello::isValid() const {
    return isSafeScalar(m_nodeId) &&
           isSafeScalar(m_publicKeyFingerprint) &&
           isHex(m_nonceHex) &&
           m_createdAt > 0;
}

std::string EncryptedPeerHandshakeHello::challengePayload() const {
    std::ostringstream output;
    output << "EncryptedPeerHandshakeHelloChallenge{"
           << "nodeId=" << m_nodeId
           << ";publicKeyFingerprint=" << m_publicKeyFingerprint
           << ";nonceHex=" << m_nonceHex
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

std::string EncryptedPeerHandshakeHello::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerHandshakeHello{"
           << "nodeId=" << m_nodeId
           << ";publicKeyFingerprint=" << m_publicKeyFingerprint
           << ";nonceHex=" << m_nonceHex
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

EncryptedPeerHandshakeAccept::EncryptedPeerHandshakeAccept()
    : m_responderNodeId(),
      m_responderFingerprint(),
      m_initiatorNonceHex(),
      m_responderNonceHex(),
      m_sessionId(),
      m_authenticationProofHex(),
      m_createdAt(0) {}

EncryptedPeerHandshakeAccept::EncryptedPeerHandshakeAccept(
    std::string responderNodeId,
    std::string responderFingerprint,
    std::string initiatorNonceHex,
    std::string responderNonceHex,
    std::string sessionId,
    std::string authenticationProofHex,
    std::int64_t createdAt
) : m_responderNodeId(std::move(responderNodeId)),
    m_responderFingerprint(std::move(responderFingerprint)),
    m_initiatorNonceHex(std::move(initiatorNonceHex)),
    m_responderNonceHex(std::move(responderNonceHex)),
    m_sessionId(std::move(sessionId)),
    m_authenticationProofHex(std::move(authenticationProofHex)),
    m_createdAt(createdAt) {}

const std::string& EncryptedPeerHandshakeAccept::responderNodeId() const { return m_responderNodeId; }
const std::string& EncryptedPeerHandshakeAccept::responderFingerprint() const { return m_responderFingerprint; }
const std::string& EncryptedPeerHandshakeAccept::initiatorNonceHex() const { return m_initiatorNonceHex; }
const std::string& EncryptedPeerHandshakeAccept::responderNonceHex() const { return m_responderNonceHex; }
const std::string& EncryptedPeerHandshakeAccept::sessionId() const { return m_sessionId; }
const std::string& EncryptedPeerHandshakeAccept::authenticationProofHex() const { return m_authenticationProofHex; }
std::int64_t EncryptedPeerHandshakeAccept::createdAt() const { return m_createdAt; }

bool EncryptedPeerHandshakeAccept::isValid() const {
    return isSafeScalar(m_responderNodeId) &&
           isSafeScalar(m_responderFingerprint) &&
           isHex(m_initiatorNonceHex) &&
           isHex(m_responderNonceHex) &&
           isHex(m_sessionId) &&
           isHex(m_authenticationProofHex) &&
           m_createdAt > 0;
}

std::string EncryptedPeerHandshakeAccept::proofPayload() const {
    std::ostringstream output;
    output << "EncryptedPeerHandshakeAcceptProof{"
           << "responderNodeId=" << m_responderNodeId
           << ";responderFingerprint=" << m_responderFingerprint
           << ";initiatorNonceHex=" << m_initiatorNonceHex
           << ";responderNonceHex=" << m_responderNonceHex
           << ";sessionId=" << m_sessionId
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

std::string EncryptedPeerHandshakeAccept::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerHandshakeAccept{"
           << "responderNodeId=" << m_responderNodeId
           << ";responderFingerprint=" << m_responderFingerprint
           << ";initiatorNonceHex=" << m_initiatorNonceHex
           << ";responderNonceHex=" << m_responderNonceHex
           << ";sessionId=" << m_sessionId
           << ";authenticationProofHex=" << m_authenticationProofHex
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

EncryptedPeerHandshakeResult::EncryptedPeerHandshakeResult()
    : m_status(EncryptedPeerHandshakeStatus::REJECTED),
      m_reason("Uninitialized encrypted peer handshake result."),
      m_sessionId() {}

EncryptedPeerHandshakeResult::EncryptedPeerHandshakeResult(
    EncryptedPeerHandshakeStatus status,
    std::string reason,
    std::string sessionId
) : m_status(status),
    m_reason(std::move(reason)),
    m_sessionId(std::move(sessionId)) {}

EncryptedPeerHandshakeStatus EncryptedPeerHandshakeResult::status() const { return m_status; }
const std::string& EncryptedPeerHandshakeResult::reason() const { return m_reason; }
const std::string& EncryptedPeerHandshakeResult::sessionId() const { return m_sessionId; }
bool EncryptedPeerHandshakeResult::accepted() const { return m_status == EncryptedPeerHandshakeStatus::ACCEPTED; }

std::string EncryptedPeerHandshakeResult::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerHandshakeResult{status="
           << (accepted() ? "ACCEPTED" : "REJECTED")
           << ";reason=" << m_reason
           << ";sessionId=" << m_sessionId
           << "}";
    return output.str();
}

EncryptedPeerHandshakeHello EncryptedPeerHandshakeManager::createHello(
    const std::string& nodeId,
    const std::string& publicKeyFingerprint,
    std::int64_t now
) {
    const std::string nonceHex = hashText(
        nodeId + "|" + publicKeyFingerprint + "|" + std::to_string(now),
        NONCE_DOMAIN
    );

    return EncryptedPeerHandshakeHello(
        nodeId,
        publicKeyFingerprint,
        nonceHex,
        now
    );
}

EncryptedPeerHandshakeAccept EncryptedPeerHandshakeManager::createAccept(
    const EncryptedPeerHandshakeHello& hello,
    const std::string& responderNodeId,
    const std::string& responderFingerprint,
    const std::string& sharedSecret,
    std::int64_t now
) {
    if (!hello.isValid()) {
        return EncryptedPeerHandshakeAccept();
    }

    const std::string responderNonceHex = hashText(
        responderNodeId + "|" + responderFingerprint + "|" +
            hello.nonceHex() + "|" + std::to_string(now),
        NONCE_DOMAIN
    );

    const std::string sessionId = EncryptedPeerSession::deriveSessionId(
        hello.nodeId(),
        responderNodeId,
        sharedSecret
    );

    const std::string proof = buildProof(
        hello,
        responderNodeId,
        responderFingerprint,
        responderNonceHex,
        sessionId,
        sharedSecret
    );

    return EncryptedPeerHandshakeAccept(
        responderNodeId,
        responderFingerprint,
        hello.nonceHex(),
        responderNonceHex,
        sessionId,
        proof,
        now
    );
}

EncryptedPeerHandshakeResult EncryptedPeerHandshakeManager::validateAccept(
    const EncryptedPeerHandshakeHello& hello,
    const EncryptedPeerHandshakeAccept& accept,
    const std::string& expectedResponderNodeId,
    const std::string& expectedResponderFingerprint,
    const std::string& sharedSecret
) {
    if (!hello.isValid() || !accept.isValid()) {
        return EncryptedPeerHandshakeResult(
            EncryptedPeerHandshakeStatus::REJECTED,
            "Encrypted peer handshake messages are invalid.",
            ""
        );
    }

    if (accept.responderNodeId() != expectedResponderNodeId ||
        accept.responderFingerprint() != expectedResponderFingerprint) {
        return EncryptedPeerHandshakeResult(
            EncryptedPeerHandshakeStatus::REJECTED,
            "Encrypted peer handshake responder identity does not match expectation.",
            ""
        );
    }

    if (accept.initiatorNonceHex() != hello.nonceHex()) {
        return EncryptedPeerHandshakeResult(
            EncryptedPeerHandshakeStatus::REJECTED,
            "Encrypted peer handshake initiator nonce does not match.",
            ""
        );
    }

    const std::string expectedSessionId = EncryptedPeerSession::deriveSessionId(
        hello.nodeId(),
        accept.responderNodeId(),
        sharedSecret
    );

    if (accept.sessionId() != expectedSessionId) {
        return EncryptedPeerHandshakeResult(
            EncryptedPeerHandshakeStatus::REJECTED,
            "Encrypted peer handshake session id is invalid.",
            ""
        );
    }

    const std::string expectedProof = buildProof(
        hello,
        accept.responderNodeId(),
        accept.responderFingerprint(),
        accept.responderNonceHex(),
        accept.sessionId(),
        sharedSecret
    );

    if (accept.authenticationProofHex() != expectedProof) {
        return EncryptedPeerHandshakeResult(
            EncryptedPeerHandshakeStatus::REJECTED,
            "Encrypted peer handshake authentication proof is invalid.",
            ""
        );
    }

    return EncryptedPeerHandshakeResult(
        EncryptedPeerHandshakeStatus::ACCEPTED,
        "Encrypted peer handshake accepted.",
        expectedSessionId
    );
}

} // namespace nodo::p2p
