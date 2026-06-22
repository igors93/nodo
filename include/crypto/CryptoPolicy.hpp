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
     * Política localnet atual.
     * Permite Ed25519 para usuários e BLS12-381 para validadores.
     */
    static CryptoPolicy developmentPolicy();

    bool isAlgorithmAllowed(
        CryptoAlgorithm algorithm,
        SecurityContext context
    ) const;

    bool developmentMode() const;

private:
    explicit CryptoPolicy(bool developmentMode);

    bool m_developmentMode;
};

} // namespace nodo::crypto

#endif
