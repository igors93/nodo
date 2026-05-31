#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::Address;
using nodo::crypto::AddressDerivation;
using nodo::crypto::KeyPair;
using nodo::crypto::PublicKey;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

PublicKey publicKeyA() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "igor-address-derivation-key"
    ).publicKey();
}

PublicKey publicKeyB() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "ana-address-derivation-key"
    ).publicKey();
}

void testAddressDerivationIsDeterministic() {
    const Address first =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    const Address second =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    requireCondition(
        first.isValid(),
        "Derived address is invalid."
    );

    requireCondition(
        first.value() == second.value(),
        "Address derivation is not deterministic."
    );
}

void testDifferentPublicKeysProduceDifferentAddresses() {
    const Address first =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    const Address second =
        AddressDerivation::deriveFromPublicKey(publicKeyB());

    requireCondition(
        first.value() != second.value(),
        "Different public keys produced the same address."
    );
}

void testAddressFormat() {
    const Address address =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    requireCondition(
        address.value().rfind(Address::networkPrefix(), 0) == 0,
        "Address does not start with Nodo network prefix."
    );

    requireCondition(
        address.value().size() == Address::totalSize(),
        "Address has an unexpected length."
    );

    for (std::size_t i = Address::networkPrefix().size();
         i < address.value().size();
         ++i) {
        const char current = address.value()[i];

        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';

        requireCondition(
            isDigit || isLowerHex,
            "Address contains a non-lowercase-hex character."
        );
    }
}

void testChecksumRejectsTamperedAddress() {
    const Address original =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    std::string tamperedValue = original.value();

    tamperedValue[tamperedValue.size() - 1] =
        tamperedValue.back() == '0' ? '1' : '0';

    const Address tampered =
        Address::fromString(tamperedValue);

    requireCondition(
        !tampered.isValid(),
        "Address accepted a tampered checksum."
    );
}

void testVerifyAddressForPublicKey() {
    const Address address =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    requireCondition(
        AddressDerivation::verifyAddressForPublicKey(
            address,
            publicKeyA()
        ),
        "Address verification failed for the matching public key."
    );

    requireCondition(
        !AddressDerivation::verifyAddressForPublicKey(
            address,
            publicKeyB()
        ),
        "Address verification accepted the wrong public key."
    );
}

void testInvalidPublicKeyIsRejected() {
    bool rejected = false;

    try {
        (void)AddressDerivation::deriveFromPublicKey(PublicKey());
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Address derivation accepted an invalid public key."
    );
}

void testAddressSerialization() {
    const Address address =
        AddressDerivation::deriveFromPublicKey(publicKeyA());

    requireCondition(
        address.serialize() ==
            "Address{value=" + address.value() + "}",
        "Address serialization is not deterministic."
    );
}

} // namespace

int main() {
    try {
        testAddressDerivationIsDeterministic();
        testDifferentPublicKeysProduceDifferentAddresses();
        testAddressFormat();
        testChecksumRejectsTamperedAddress();
        testVerifyAddressForPublicKey();
        testInvalidPublicKeyIsRejected();
        testAddressSerialization();

        std::cout << "Nodo address derivation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo address derivation tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
