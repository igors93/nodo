#include "crypto/CryptoPolicy.hpp"

namespace nodo::crypto {

CryptoPolicy::CryptoPolicy(
    bool developmentMode,
    bool requireHybridForCriticalOperations
)
    : m_developmentMode(developmentMode),
      m_requireHybridForCriticalOperations(requireHybridForCriticalOperations) {}

CryptoPolicy CryptoPolicy::developmentPolicy() {
    return CryptoPolicy(
        true,
        false
    );
}

CryptoPolicy CryptoPolicy::futureHybridPolicy() {
    return CryptoPolicy(
        false,
        true
    );
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

    if (context == SecurityContext::VALIDATOR_OPERATION) {
        return algorithm == CryptoAlgorithm::BLS12_381;
    }

    if (context == SecurityContext::TREASURY_OPERATION ||
        context == SecurityContext::MINT_OPERATION) {
        return algorithm == CryptoAlgorithm::BLS12_381;
    }

    /*
     * Classic algorithms are allowed by the current architecture.
     * Real mathematical verification will be implemented by crypto providers.
     */
    if (isClassicAlgorithm(algorithm)) {
        return true;
    }

    if (isValidatorAlgorithm(algorithm)) {
        return true;
    }

    /*
     * Post-quantum algorithms are known by the architecture,
     * but they must not be considered secure until providers exist.
     */
    if (isPostQuantumAlgorithm(algorithm)) {
        return true;
    }

    /*
     * HYBRID_CLASSIC_AND_POST_QUANTUM is not an individual signature.
     * It is a policy requirement for a SignatureBundle.
     */
    if (algorithm == CryptoAlgorithm::HYBRID_CLASSIC_AND_POST_QUANTUM) {
        return false;
    }

    return false;
}

bool CryptoPolicy::requiresHybridSignature(SecurityContext context) const {
    if (!m_requireHybridForCriticalOperations) {
        return false;
    }

    return context == SecurityContext::VALIDATOR_OPERATION ||
           context == SecurityContext::TREASURY_OPERATION ||
           context == SecurityContext::MINT_OPERATION;
}

bool CryptoPolicy::developmentMode() const {
    return m_developmentMode;
}

} // namespace nodo::crypto
