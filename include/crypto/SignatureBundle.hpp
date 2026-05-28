#ifndef NODO_CRYPTO_SIGNATURE_BUNDLE_HPP
#define NODO_CRYPTO_SIGNATURE_BUNDLE_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::crypto {

/*
 * SignatureBundle é um pacote de assinaturas.
 *
 * Ideia central:
 * Hoje ele pode carregar uma assinatura clássica.
 * No futuro ele poderá carregar:
 *
 * - assinatura clássica;
 * - assinatura pós-quântica;
 * - ambas ao mesmo tempo.
 */
class SignatureBundle {
public:
    SignatureBundle();

    void addSignature(const Signature& signature);

    const std::vector<Signature>& signatures() const;

    bool empty() const;

    bool hasAlgorithm(CryptoAlgorithm algorithm) const;

    /*
     * Verifica se o conjunto de assinaturas respeita a política da rede.
     *
     * IMPORTANTE:
     * Isto ainda não verifica matematicamente a assinatura.
     * Por enquanto, valida estrutura e política.
     *
     * A verificação real entrará depois com provedores criptográficos.
     */
    bool isValidForPolicy(
        const CryptoPolicy& policy,
        SecurityContext context
    ) const;

    std::string serialize() const;

    /*
     * Cria uma assinatura fake de desenvolvimento.
     *
     * NÃO É SEGURA.
     * Serve apenas para testar o fluxo:
     * mensagem -> assinatura -> bundle -> política.
     */
    static SignatureBundle createDevelopmentSignature(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp
    );

private:
    std::vector<Signature> m_signatures;
};

} // namespace nodo::crypto

#endif