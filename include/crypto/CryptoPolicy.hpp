#ifndef NODO_CRYPTO_CRYPTO_POLICY_HPP
#define NODO_CRYPTO_CRYPTO_POLICY_HPP

#include "crypto/CryptoAlgorithm.hpp"

namespace nodo::crypto {

/*
 * SecurityContext define o tipo de ação que está sendo protegida.
 *
 * Algumas ações são mais sensíveis que outras.
 * Exemplo:
 * - transação comum é importante;
 * - operação de validador é mais importante;
 * - emissão de moeda é extremamente importante.
 */
enum class SecurityContext {
    USER_TRANSACTION,
    VALIDATOR_OPERATION,
    TREASURY_OPERATION,
    MINT_OPERATION,
    DEVELOPMENT_ONLY
};

/*
 * CryptoPolicy decide quais algoritmos são permitidos.
 *
 * DECISÃO DE SEGURANÇA:
 * O algoritmo pode existir no código, mas a política decide
 * se ele pode ou não ser usado na rede.
 */
class CryptoPolicy {
public:
    /*
     * Política usada agora, no começo do projeto.
     * Permite assinatura fake apenas para desenvolvimento.
     */
    static CryptoPolicy developmentPolicy();

    /*
     * Política futura: exigirá assinatura híbrida em operações críticas.
     */
    static CryptoPolicy futureHybridPolicy();

    bool isAlgorithmAllowed(
        CryptoAlgorithm algorithm,
        SecurityContext context
    ) const;

    bool requiresHybridSignature(SecurityContext context) const;

    bool developmentMode() const;

private:
    CryptoPolicy(
        bool developmentMode,
        bool requireHybridForCriticalOperations
    );

    bool m_developmentMode;
    bool m_requireHybridForCriticalOperations;
};

} // namespace nodo::crypto

#endif