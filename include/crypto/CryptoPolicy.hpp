#ifndef NODO_CRYPTO_CRYPTO_POLICY_HPP
#define NODO_CRYPTO_CRYPTO_POLICY_HPP

#include "crypto/CryptoAlgorithm.hpp"

namespace nodo::crypto {

/* SecurityContext describes the operation protected by a signature. */
enum class SecurityContext {
    PEER_AUTHENTICATION,
    USER_TRANSACTION,
    VALIDATOR_OPERATION,
    TREASURY_OPERATION,
    MINT_OPERATION,
    DEVELOPMENT_ONLY
};

/* CryptoPolicy selects which algorithms are permitted for each context. */
class CryptoPolicy {
public:
    /* Development policy permits Ed25519 user signatures and BLS12-381 validator signatures. */
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
