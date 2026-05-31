#ifndef NODO_CRYPTO_SIGNATURE_HPP
#define NODO_CRYPTO_SIGNATURE_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * Signature represents one signature produced by one algorithm.
 *
 * A SignatureBundle can contain more than one signature. This prepares Nodo for
 * future hybrid models:
 *
 * classic signature + post-quantum signature
 */
class Signature {
public:
    Signature();

    Signature(
        CryptoAlgorithm algorithm,
        PublicKey publicKey,
        std::string signatureHex,
        std::int64_t createdAt
    );

    Signature(
        CryptoSuiteId suite,
        SigningDomain domain,
        CryptoAlgorithm algorithm,
        PublicKey publicKey,
        std::string signatureHex,
        std::int64_t createdAt
    );

    CryptoSuiteId suite() const;
    SigningDomain domain() const;
    CryptoAlgorithm algorithm() const;
    const PublicKey& publicKey() const;
    const std::string& signatureHex() const;
    std::int64_t createdAt() const;

    bool isValid() const;

    std::string serialize() const;

private:
    static bool isSafeSignatureHex(
        const std::string& value
    );

    CryptoSuiteId m_suite;
    SigningDomain m_domain;
    CryptoAlgorithm m_algorithm;
    PublicKey m_publicKey;
    std::string m_signatureHex;
    std::int64_t m_createdAt;
};

} // namespace nodo::crypto

#endif
