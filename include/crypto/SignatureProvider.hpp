#ifndef NODO_CRYPTO_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_SIGNATURE_PROVIDER_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureVerificationResult.hpp"

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
 * Current status:
 * The first concrete provider is DevelopmentSignatureProvider. It is not
 * production cryptography. It exists to make the boundary real before adding
 * audited algorithm providers.
 */
class SignatureProvider {
public:
    virtual ~SignatureProvider() = default;

    virtual CryptoAlgorithm algorithm() const = 0;

    virtual Signature sign(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp
    ) const = 0;

    virtual SignatureVerificationResult verify(
        const std::string& message,
        const Signature& signature
    ) const = 0;
};

} // namespace nodo::crypto

#endif
