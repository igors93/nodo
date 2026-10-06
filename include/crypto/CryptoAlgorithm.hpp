#ifndef NODO_CRYPTO_CRYPTO_ALGORITHM_HPP
#define NODO_CRYPTO_CRYPTO_ALGORITHM_HPP

#include <string>

namespace nodo::crypto {

/* Algorithms recognized by Nodo. Availability does not imply production approval. */
enum class CryptoAlgorithm {
    /* Development-only fake signing is never safe for a live network. */
    DEVELOPMENT_FAKE_SIGNATURE,

    /* Classical algorithms; availability depends on provider support. */
    CLASSIC_ED25519,
    CLASSIC_ECDSA_SECP256K1,

    /* Validator consensus signature algorithm. */
    BLS12_381
};

std::string cryptoAlgorithmToString(CryptoAlgorithm algorithm);
CryptoAlgorithm cryptoAlgorithmFromString(const std::string& value);

bool isClassicAlgorithm(CryptoAlgorithm algorithm);
bool isValidatorAlgorithm(CryptoAlgorithm algorithm);
bool isDevelopmentOnlyAlgorithm(CryptoAlgorithm algorithm);

} // namespace nodo::crypto

#endif
