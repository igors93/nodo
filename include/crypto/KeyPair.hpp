#ifndef NODO_CRYPTO_KEY_PAIR_HPP
#define NODO_CRYPTO_KEY_PAIR_HPP

#include "crypto/Address.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * KeyPair is the first key management boundary in Nodo.
 *
 * In simple terms:
 *
 *   PrivateKey + PublicKey -> KeyPair -> Address + signatures
 *
 * Current status:
 * This is a development foundation. It does not store keys securely yet and it
 * does not implement a wallet. It creates one official place where code can
 * handle a public/private key pair before future secure key storage is added.
 */
class KeyPair {
public:
    KeyPair();

    KeyPair(
        PublicKey publicKey,
        PrivateKey privateKey
    );

    /*
     * Development-only helper.
     *
     * It creates deterministic key material from a seed so tests and demos can
     * reproduce the same key pair. This is NOT secure key generation.
     */
    static KeyPair createDevelopmentKeyPair(
        const std::string& identitySeed
    );

    CryptoAlgorithm algorithm() const;

    const PublicKey& publicKey() const;

    /*
     * SECURITY WARNING:
     * This exposes the private key object only for signing/provider boundaries.
     * Future production code should replace this with secure key storage.
     */
    const PrivateKey& privateKeyForSigningOnly() const;

    Address address() const;

    bool isValid() const;

    /*
     * Checks whether this key pair can sign a provider challenge and verify it
     * through the same provider.
     */
    bool canSignAndVerify(
        const SignatureProvider& provider
    ) const;

    SignatureBundle sign(
        const std::string& message,
        std::int64_t timestamp,
        const SignatureProvider& provider
    ) const;

    /*
     * Public identity only.
     *
     * This intentionally does not include private key material.
     */
    std::string publicIdentity() const;

    /*
     * Deterministic public serialization only.
     *
     * This intentionally does not include private key material.
     */
    std::string serializePublic() const;

private:
    static std::string deriveDevelopmentKeyMaterial(
        const std::string& domain,
        const std::string& identitySeed
    );

    static std::string signingChallenge(
        const PublicKey& publicKey
    );

    PublicKey m_publicKey;
    PrivateKey m_privateKey;
};

} // namespace nodo::crypto

#endif
