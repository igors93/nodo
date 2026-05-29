#ifndef NODO_CRYPTO_DEVELOPMENT_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_DEVELOPMENT_SIGNATURE_PROVIDER_HPP

#include "crypto/SignatureProvider.hpp"

#include <string>

namespace nodo::crypto {

/*
 * DevelopmentSignatureProvider is a deterministic development-only provider.
 *
 * IMPORTANT:
 * This is not a real digital signature algorithm.
 *
 * It exists so the rest of Nodo can use the same provider interface that later
 * real providers will use.
 */
class DevelopmentSignatureProvider final : public SignatureProvider {
public:
    CryptoAlgorithm algorithm() const override;

    Signature sign(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp
    ) const override;

    SignatureVerificationResult verify(
        const std::string& message,
        const Signature& signature
    ) const override;

private:
    static std::string computeDevelopmentSignatureHex(
        const std::string& message,
        const PublicKey& publicKey
    );
};

} // namespace nodo::crypto

#endif
