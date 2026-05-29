#include "crypto/AddressDerivation.hpp"

#include "crypto/hash.h"

#include <stdexcept>

namespace nodo::crypto {

Address AddressDerivation::deriveFromPublicKey(
    const PublicKey& publicKey
) {
    if (!publicKey.isValid()) {
        throw std::invalid_argument("Cannot derive address from invalid public key.");
    }

    const std::string body =
        Address::networkPrefix() +
        computeAddressPayloadHex(publicKey);

    const Address address(
        body + computeChecksum(body)
    );

    if (!address.isValid()) {
        throw std::logic_error("Derived address is invalid.");
    }

    return address;
}

bool AddressDerivation::verifyAddressForPublicKey(
    const Address& address,
    const PublicKey& publicKey
) {
    if (!address.isValid()) {
        return false;
    }

    if (!publicKey.isValid()) {
        return false;
    }

    return address.value() == deriveFromPublicKey(publicKey).value();
}

std::string AddressDerivation::computeAddressPayloadHex(
    const PublicKey& publicKey
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        "NODO_ADDRESS_DERIVATION_V1|" +
        publicKey.serialize();

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output).substr(0, Address::payloadHexSize());
}

std::string AddressDerivation::computeChecksum(
    const std::string& body
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        "NODO_ADDRESS_CHECKSUM_V1|" + body;

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output).substr(0, Address::checksumHexSize());
}

} // namespace nodo::crypto
