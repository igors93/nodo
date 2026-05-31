#ifndef NODO_CRYPTO_BLS12381_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_BLS12381_SIGNATURE_PROVIDER_HPP

#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"

#include <string>
#include <vector>

namespace nodo::crypto {

class Bls12381SignatureProvider final : public SignatureProvider {
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

    static Signature aggregateSignatures(
        const std::vector<Signature>& signatures,
        std::int64_t createdAt
    );
};

} // namespace nodo::crypto

#endif
