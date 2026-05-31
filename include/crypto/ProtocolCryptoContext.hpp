#ifndef NODO_CRYPTO_PROTOCOL_CRYPTO_CONTEXT_HPP
#define NODO_CRYPTO_PROTOCOL_CRYPTO_CONTEXT_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/LocalSignatureProvider.hpp"
#include "crypto/SignatureProvider.hpp"

#include <string>

namespace nodo::crypto {

/*
 * ProtocolCryptoContext defines the cryptographic boundary used by protocol
 * execution.
 *
 * Runtime code should depend on this context instead of choosing a crypto
 * policy or signature provider inline. This keeps localnet, testnet and mainnet
 * on one protocol path while allowing each network profile to enforce different
 * cryptographic requirements.
 */
class ProtocolCryptoContext {
public:
    /*
     * Current runnable profile.
     *
     * localnet still uses the temporary LocalSignatureProvider, but this makes
     * that choice explicit and isolated behind a protocol context.
     */
    static ProtocolCryptoContext localnet();

    const std::string& networkProfile() const;

    const CryptoPolicy& policy() const;

    const SignatureProvider& signatureProvider() const;

    bool productionSafe() const;

    bool isValid() const;

private:
    ProtocolCryptoContext(
        std::string networkProfile,
        CryptoPolicy policy,
        bool productionSafe
    );

    std::string m_networkProfile;
    CryptoPolicy m_policy;
    bool m_productionSafe;
    LocalSignatureProvider m_localProvider;
};

} // namespace nodo::crypto

#endif
