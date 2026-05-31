#ifndef NODO_CRYPTO_LOCAL_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_LOCAL_SIGNATURE_PROVIDER_HPP

#include "crypto/DevelopmentSignatureProvider.hpp"

namespace nodo::crypto {

/*
 * LocalSignatureProvider is the explicit temporary provider for localnet.
 *
 * It wraps the deterministic development provider so protocol code depends on
 * a named localnet boundary instead of scattering development-signature details
 * through runtime and CLI code.
 */
class LocalSignatureProvider final : public SignatureProvider {
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
    DevelopmentSignatureProvider m_developmentProvider;
};

} // namespace nodo::crypto

#endif
