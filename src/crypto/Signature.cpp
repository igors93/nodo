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

    if (m_signatureHex.empty()) {
        return false;
    }

    if (m_createdAt <= 0) {
        return false;
    }

    /*
     * A assinatura deve declarar o mesmo algoritmo da chave pública.
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

} // namespace nodo::crypto