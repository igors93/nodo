#ifndef NODO_CRYPTO_ED25519_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_ED25519_SIGNATURE_PROVIDER_HPP

#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"

#include <string>

namespace nodo::crypto {

class Ed25519SignatureProvider final : public SignatureProvider {
public:
    CryptoAlgorithm algorithm() const override;

    Signature sign(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp,
        SigningDomain domain
    ) const override;

    SignatureVerificationResult verify(
        const std::string& message,
        const Signature& signature
    ) const override;

    static KeyPair generateKeyPair();

    static KeyPair deriveKeyPairFromSeed(
        const std::string& seed
    );

    static bool isValidPublicKeyMaterial(
        const std::string& keyMaterial
    );

    static bool isValidPrivateKeyMaterial(
        const std::string& keyMaterial
    );
};

} // namespace nodo::crypto

#endif
