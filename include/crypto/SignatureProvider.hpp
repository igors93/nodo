#ifndef NODO_CRYPTO_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_SIGNATURE_PROVIDER_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureVerificationResult.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * SignatureProvider is the official boundary for signing and verification.
 *
 * In simple terms:
 * Blockchain code should not know the mathematical details of Ed25519,
 * ECDSA, ML-DSA, or future algorithms. It should ask a provider:
 *
 * - sign this message;
 * - verify this signature.
 *
 * Current providers:
 * - Ed25519 through OpenSSL for user transaction signatures.
 * - BLS12-381 through blst for validator votes and block proposals.
 */
class SignatureProvider {
public:
    virtual ~SignatureProvider() = default;

    virtual CryptoAlgorithm algorithm() const = 0;

    virtual Signature sign(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp,
        SigningDomain domain
    ) const = 0;

    virtual SignatureVerificationResult verify(
        const std::string& message,
        const Signature& signature
    ) const = 0;
};

} // namespace nodo::crypto

#endif
