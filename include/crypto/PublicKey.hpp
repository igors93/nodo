#ifndef NODO_CRYPTO_PUBLIC_KEY_HPP
#define NODO_CRYPTO_PUBLIC_KEY_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <string>

namespace nodo::crypto {

/*
 * PublicKey representa uma chave pública.
 *
 * A chave pública pode ser revelada para a rede.
 * Ela serve para verificar se uma assinatura veio do dono correto.
 */
class PublicKey {
public:
    PublicKey();

    PublicKey(
        CryptoAlgorithm algorithm,
        std::string keyMaterial
    );

    CryptoAlgorithm algorithm() const;
    const std::string& keyMaterial() const;

    bool isValid() const;

    /*
     * Fingerprint é uma "impressão digital" curta da chave.
     * No futuro, endereços da Nodo poderão ser derivados de algo assim.
     */
    std::string fingerprint() const;

    /*
     * Serialização determinística.
     *
     * REGRA:
     * Tudo que entra em hash/assinatura deve sempre ser serializado
     * na mesma ordem.
     */
    std::string serialize() const;

private:
    CryptoAlgorithm m_algorithm;
    std::string m_keyMaterial;
};

} // namespace nodo::crypto

#endif