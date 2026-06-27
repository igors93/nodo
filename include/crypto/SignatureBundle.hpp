#ifndef NODO_CRYPTO_SIGNATURE_BUNDLE_HPP
#define NODO_CRYPTO_SIGNATURE_BUNDLE_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::crypto {

/*
 * SignatureBundle is a package of signatures.
 *
 * Today it usually carries one domain-separated signature. In future versions it can
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
     * Reconstructs a SignatureBundle from its canonical, self-contained form.
     * Every serialized Signature carries the complete public key required for
     * verification. Throws std::invalid_argument on parse/canonicality errors.
     */
    static SignatureBundle deserialize(const std::string& serialized);

    /*
     * Creates a signature through a provider boundary.
     */
    static SignatureBundle createSignature(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp,
        const SignatureProvider& provider,
        SigningDomain domain
    );

private:
    std::vector<Signature> m_signatures;
};

} // namespace nodo::crypto

#endif
