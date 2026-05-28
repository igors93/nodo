#ifndef NODO_CRYPTO_SIGNATURE_HPP
#define NODO_CRYPTO_SIGNATURE_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * Signature representa uma assinatura individual.
 *
 * Um SignatureBundle pode conter várias assinaturas.
 * Isso prepara a Nodo para o modelo híbrido:
 *
 * assinatura clássica + assinatura pós-quântica
 */
class Signature {
public:
    Signature();

    Signature(
        CryptoAlgorithm algorithm,
        PublicKey publicKey,
        std::string signatureHex,
        std::int64_t createdAt
    );

    CryptoAlgorithm algorithm() const;
    const PublicKey& publicKey() const;
    const std::string& signatureHex() const;
    std::int64_t createdAt() const;

    bool isValid() const;

    std::string serialize() const;

private:
    CryptoAlgorithm m_algorithm;
    PublicKey m_publicKey;
    std::string m_signatureHex;
    std::int64_t m_createdAt;
};

} // namespace nodo::crypto

#endif