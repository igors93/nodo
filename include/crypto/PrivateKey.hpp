#ifndef NODO_CRYPTO_PRIVATE_KEY_HPP
#define NODO_CRYPTO_PRIVATE_KEY_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <string>

namespace nodo::crypto {

/* Private key material must never be logged or transmitted. This type models a signing boundary. */
class PrivateKey {
public:
    PrivateKey();

    PrivateKey(
        CryptoAlgorithm algorithm,
        std::string keyMaterial
    );

    CryptoAlgorithm algorithm() const;

    bool isValid() const;

    /* Expose key material only to the signing implementation. Never print or log it. */
    const std::string& keyMaterialForSigningOnly() const;

private:
    CryptoAlgorithm m_algorithm;
    std::string m_keyMaterial;
};

} // namespace nodo::crypto

#endif
