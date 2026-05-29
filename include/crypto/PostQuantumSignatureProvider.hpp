#ifndef NODO_CRYPTO_POST_QUANTUM_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_POST_QUANTUM_SIGNATURE_PROVIDER_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * PostQuantumSignatureProvider is the interface future post-quantum
 * signature implementations must follow.
 *
 * IMPORTANT:
 * This file does not implement ML-DSA, SLH-DSA, or any real post-quantum
 * cryptography. It only defines the provider boundary so real audited
 * providers can be connected later without changing the blockchain core.
 */
class PostQuantumSignatureProvider : public SignatureProvider {
public:
    ~PostQuantumSignatureProvider() override = default;

    /*
     * Human-readable provider name.
     *
     * Example future values:
     * - "liboqs-ml-dsa"
     * - "hardware-pq-provider"
     */
    virtual std::string providerName() const = 0;

    /*
     * Human-readable algorithm family.
     *
     * Example:
     * - "ML-DSA"
     * - "SLH-DSA"
     */
    virtual std::string algorithmFamily() const = 0;

    /*
     * Claimed NIST security level for the provider profile.
     *
     * This is metadata only. It does not replace third-party review.
     */
    virtual std::uint32_t claimedSecurityLevel() const = 0;

    virtual std::uint32_t publicKeySizeBytes() const = 0;
    virtual std::uint32_t privateKeySizeBytes() const = 0;
    virtual std::uint32_t signatureSizeBytes() const = 0;

    /*
     * A provider must explicitly say whether it is production ready.
     *
     * Development placeholders must return false.
     */
    virtual bool isProductionReady() const = 0;
};

} // namespace nodo::crypto

#endif
