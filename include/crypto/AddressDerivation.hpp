#ifndef NODO_CRYPTO_ADDRESS_DERIVATION_HPP
#define NODO_CRYPTO_ADDRESS_DERIVATION_HPP

#include "crypto/Address.hpp"
#include "crypto/PublicKey.hpp"

namespace nodo::crypto {

/*
 * AddressDerivation creates deterministic Nodo addresses from public keys.
 *
 * In simple terms:
 *   PublicKey -> SHA-256 based derivation -> Address
 */
class AddressDerivation {
public:
    static Address deriveFromPublicKey(
        const PublicKey& publicKey
    );

    static bool verifyAddressForPublicKey(
        const Address& address,
        const PublicKey& publicKey
    );

private:
    static std::string computeAddressPayloadHex(
        const PublicKey& publicKey
    );

    static std::string computeChecksum(
        const std::string& body
    );
};

} // namespace nodo::crypto

#endif
