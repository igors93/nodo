#include "crypto/Signature.hpp"

#include <sstream>
#include <utility>

namespace nodo::crypto {

Signature::Signature()
    : m_suite(CryptoSuiteId::UNKNOWN),
      m_domain(SigningDomain::UNKNOWN),
      m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_publicKey(),
      m_signatureHex(""),
      m_createdAt(0) {}

Signature::Signature(
    CryptoAlgorithm algorithm,
    PublicKey publicKey,
    std::string signatureHex,
    std::int64_t createdAt
)
    : Signature(
          CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
          SigningDomain::USER_TRANSACTION,
          algorithm,
          std::move(publicKey),
          std::move(signatureHex),
          createdAt
      ) {}

Signature::Signature(
    CryptoSuiteId suite,
    SigningDomain domain,
    CryptoAlgorithm algorithm,
    PublicKey publicKey,
    std::string signatureHex,
    std::int64_t createdAt
)
    : m_suite(suite),
      m_domain(domain),
      m_algorithm(algorithm),
      m_publicKey(std::move(publicKey)),
      m_signatureHex(std::move(signatureHex)),
      m_createdAt(createdAt) {}

CryptoSuiteId Signature::suite() const {
    return m_suite;
}

SigningDomain Signature::domain() const {
    return m_domain;
}

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
    if (!isSupportedCryptoSuite(m_suite)) {
        return false;
    }

    if (m_domain == SigningDomain::UNKNOWN) {
        return false;
    }

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
        << "suite=" << cryptoSuiteIdToString(m_suite)
        << ";domain=" << signingDomainToString(m_domain)
        << ";algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";publicKey=" << m_publicKey.serialize()
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
