#ifndef NODO_CRYPTO_SIGNATURE_BUNDLE_HPP
#define NODO_CRYPTO_SIGNATURE_BUNDLE_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::crypto {

/*
 * SignatureBundle is a package of signatures.
 *
 * Today it usually carries one development signature. In future versions it can
 * carry:
 *
 * - one classic signature;
 * - one post-quantum signature;
 * - both at the same time.
 */
class SignatureBundle {
public:
    SignatureBundle();

    void addSignature(const Signature& signature);

    const std::vector<Signature>& signatures() const;

    bool empty() const;

    bool hasAlgorithm(CryptoAlgorithm algorithm) const;

    /*
     * Checks structure and network policy.
     *
     * This does not perform mathematical verification by itself. Use
     * verifyForPolicy(...) when the message and provider are available.
     */
    bool isValidForPolicy(
        const CryptoPolicy& policy,
        SecurityContext context
    ) const;

    /*
     * Checks structure, network policy, and provider verification for a
     * message.
     */
    bool verifyForPolicy(
        const std::string& message,
        const CryptoPolicy& policy,
        SecurityContext context,
        const SignatureProvider& provider
    ) const;

    std::string serialize() const;

    /*
     * Creates a signature through a provider boundary.
     */
    static SignatureBundle createSignature(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp,
        const SignatureProvider& provider
    );

    /*
     * Creates a development-only signature through DevelopmentSignatureProvider.
     *
     * NOT SECURE.
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
