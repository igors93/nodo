#include "crypto/Signature.hpp"

#include <sstream>
#include <utility>

namespace nodo::crypto {

Signature::Signature()
    : m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_publicKey(),
      m_signatureHex(""),
      m_createdAt(0) {}

Signature::Signature(
    CryptoAlgorithm algorithm,
    PublicKey publicKey,
    std::string signatureHex,
    std::int64_t createdAt
)
    : m_algorithm(algorithm),
      m_publicKey(std::move(publicKey)),
      m_signatureHex(std::move(signatureHex)),
      m_createdAt(createdAt) {}

CryptoAlgorithm Signature::algorithm() const {
    return m_algorithm;
}

const PublicKey& Signature::publicKey() const {
    return m_publicKey;
}

const std::string& Signature::signatureHex() const {
    return m_signatureHex;
}

std::int64_t Signature::createdAt() const {
    return m_createdAt;
}

bool Signature::isValid() const {
    if (!m_publicKey.isValid()) {
        return false;
    }

    if (!isSafeSignatureHex(m_signatureHex)) {
        return false;
    }

    if (m_createdAt <= 0) {
        return false;
    }

    /*
     * A signature must declare the same algorithm as the public key that will
     * verify it.
     */
    if (m_algorithm != m_publicKey.algorithm()) {
        return false;
    }

    return true;
}

std::string Signature::serialize() const {
    std::ostringstream oss;

    oss << "Signature{"
        << "algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";publicKeyFingerprint=" << m_publicKey.fingerprint()
        << ";signatureHex=" << m_signatureHex
        << ";createdAt=" << m_createdAt
        << "}";

    return oss.str();
}

bool Signature::isSafeSignatureHex(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char current : value) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';
        const bool isUpperHex = current >= 'A' && current <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::crypto
