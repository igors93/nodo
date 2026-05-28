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
    /*
     * Assinatura fake só é aceita em modo desenvolvimento.
     * Isso impede que ela seja usada por acidente em modo futuro/produção.
     */
    if (isDevelopmentOnlyAlgorithm(algorithm)) {
        return m_developmentMode && context == SecurityContext::DEVELOPMENT_ONLY;
    }

    /*
     * Algoritmos clássicos são aceitos na fase inicial.
     */
    if (isClassicAlgorithm(algorithm)) {
        return true;
    }

    /*
     * Algoritmos pós-quânticos serão aceitos quando implementados.
     * Por enquanto, eles existem só como parte da arquitetura.
     */
    if (isPostQuantumAlgorithm(algorithm)) {
        return true;
    }

    /*
     * HYBRID_CLASSIC_AND_POST_QUANTUM não é uma assinatura individual.
     * É uma exigência de política para o SignatureBundle.
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