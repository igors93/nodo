#ifndef NODO_CRYPTO_CRYPTO_ALGORITHM_HPP
#define NODO_CRYPTO_CRYPTO_ALGORITHM_HPP

#include <string>

namespace nodo::crypto {

/*
 * CryptoAlgorithm define os algoritmos criptográficos que a Nodo conhece.
 *
 * DECISÃO DE SEGURANÇA:
 * A Nodo não deve depender para sempre de um único algoritmo.
 * Isso permite migrar no futuro para criptografia pós-quântica.
 */
enum class CryptoAlgorithm {
    /*
     * Apenas para desenvolvimento.
     * NÃO É SEGURO.
     * Serve para testar a arquitetura antes de integrar uma biblioteca real.
     */
    DEVELOPMENT_FAKE_SIGNATURE,

    /*
     * Algoritmos clássicos.
     * Futuramente podem ser implementados usando bibliotecas confiáveis.
     */
    CLASSIC_ED25519,
    CLASSIC_ECDSA_SECP256K1,

    /*
     * Algoritmos pós-quânticos planejados para o futuro.
     */
    POST_QUANTUM_ML_DSA,
    POST_QUANTUM_SLH_DSA,

    /*
     * Representa uma exigência de assinatura híbrida.
     * Na prática, o SignatureBundle deve conter uma assinatura clássica
     * e uma assinatura pós-quântica.
     */
    HYBRID_CLASSIC_AND_POST_QUANTUM
};

std::string cryptoAlgorithmToString(CryptoAlgorithm algorithm);
CryptoAlgorithm cryptoAlgorithmFromString(const std::string& value);

bool isClassicAlgorithm(CryptoAlgorithm algorithm);
bool isPostQuantumAlgorithm(CryptoAlgorithm algorithm);
bool isDevelopmentOnlyAlgorithm(CryptoAlgorithm algorithm);

} // namespace nodo::crypto

#endif
