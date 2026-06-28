#include "crypto/CryptoPolicy.hpp"

namespace nodo::crypto {

CryptoPolicy::CryptoPolicy(bool developmentMode)
    : m_developmentMode(developmentMode) {}

CryptoPolicy CryptoPolicy::developmentPolicy() {
    return CryptoPolicy(true);
}

bool CryptoPolicy::isAlgorithmAllowed(
    CryptoAlgorithm algorithm,
    SecurityContext context
) const {
    (void)context;

    if (isDevelopmentOnlyAlgorithm(algorithm)) {
        return context == SecurityContext::DEVELOPMENT_ONLY &&
               m_developmentMode;
    }

    if (context == SecurityContext::USER_TRANSACTION) {
        return algorithm == CryptoAlgorithm::CLASSIC_ED25519;
    }

    if (context == SecurityContext::PEER_AUTHENTICATION) {
        return algorithm == CryptoAlgorithm::CLASSIC_ED25519;
    }

    if (context == SecurityContext::VALIDATOR_OPERATION) {
        return algorithm == CryptoAlgorithm::BLS12_381;
    }

    if (context == SecurityContext::TREASURY_OPERATION ||
        context == SecurityContext::MINT_OPERATION) {
        return algorithm == CryptoAlgorithm::BLS12_381;
    }

    if (isClassicAlgorithm(algorithm)) {
        return true;
    }

    if (isValidatorAlgorithm(algorithm)) {
        return true;
    }

    return false;
}

bool CryptoPolicy::developmentMode() const {
    return m_developmentMode;
}

} // namespace nodo::crypto
