#include "crypto/LocalSignatureProvider.hpp"

namespace nodo::crypto {

CryptoAlgorithm LocalSignatureProvider::algorithm() const {
    return m_developmentProvider.algorithm();
}

Signature LocalSignatureProvider::sign(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp
) const {
    return m_developmentProvider.sign(
        message,
        publicKey,
        privateKey,
        timestamp
    );
}

SignatureVerificationResult LocalSignatureProvider::verify(
    const std::string& message,
    const Signature& signature
) const {
    return m_developmentProvider.verify(
        message,
        signature
    );
}

} // namespace nodo::crypto
