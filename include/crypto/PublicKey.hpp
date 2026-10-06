#ifndef NODO_CRYPTO_PUBLIC_KEY_HPP
#define NODO_CRYPTO_PUBLIC_KEY_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <string>

namespace nodo::crypto {

/* Public key material may be shared and used to verify signatures. */
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

    /* A short fingerprint for display and lookup. */
    std::string fingerprint() const;

    /* Serialize deterministically before hashing or signing. */
    std::string serialize() const;

private:
    CryptoAlgorithm m_algorithm;
    std::string m_keyMaterial;
};

} // namespace nodo::crypto

#endif