#ifndef NODO_CRYPTO_PRIVATE_KEY_HPP
#define NODO_CRYPTO_PRIVATE_KEY_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <string>

namespace nodo::crypto {

/*
 * PrivateKey representa uma chave privada.
 *
 * ALERTA DE SEGURANÇA:
 * Em uma blockchain real, chave privada nunca deve ser salva em texto puro,
 * nunca deve ser enviada pela rede e nunca deve aparecer em logs.
 *
 * Nesta fase inicial, ela existe apenas para modelar a arquitetura.
 */
class PrivateKey {
public:
    PrivateKey();

    PrivateKey(
        CryptoAlgorithm algorithm,
        std::string keyMaterial
    );

    CryptoAlgorithm algorithm() const;

    bool isValid() const;

    /*
     * Usado somente pela assinatura fake de desenvolvimento.
     *
     * NÃO imprimir esse valor.
     * NÃO salvar em logs.
     */
    const std::string& keyMaterialForSigningOnly() const;

private:
    CryptoAlgorithm m_algorithm;
    std::string m_keyMaterial;
};

} // namespace nodo::crypto

#endif