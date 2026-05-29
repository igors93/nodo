#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/DevelopmentSignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::Address;
using nodo::crypto::AddressDerivation;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::DevelopmentSignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SignatureBundle;

constexpr std::int64_t kTestTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testDevelopmentKeyPairIsDeterministic() {
    const KeyPair first =
        KeyPair::createDevelopmentKeyPair("igor");

    const KeyPair second =
        KeyPair::createDevelopmentKeyPair("igor");

    requireCondition(
        first.isValid(),
        "Development KeyPair is invalid."
    );

    requireCondition(
        first.publicKey().serialize() == second.publicKey().serialize(),
        "Development KeyPair public key is not deterministic."
    );

    requireCondition(
        first.address().value() == second.address().value(),
        "Development KeyPair address is not deterministic."
    );
}

void testDifferentSeedsProduceDifferentAddresses() {
    const KeyPair first =
        KeyPair::createDevelopmentKeyPair("igor");

    const KeyPair second =
        KeyPair::createDevelopmentKeyPair("ana");

    requireCondition(
        first.address().value() != second.address().value(),
        "Different development seeds produced the same address."
    );
}

void testKeyPairAddressMatchesPublicKeyDerivation() {
    const KeyPair keyPair =
        KeyPair::createDevelopmentKeyPair("address-match");

    const Address derivedAddress =
        AddressDerivation::deriveFromPublicKey(
            keyPair.publicKey()
        );

    requireCondition(
        keyPair.address().value() == derivedAddress.value(),
        "KeyPair address does not match AddressDerivation."
    );
}

void testKeyPairCanSignAndVerify() {
    const DevelopmentSignatureProvider provider;

    const KeyPair keyPair =
        KeyPair::createDevelopmentKeyPair("signing-keypair");

    requireCondition(
        keyPair.canSignAndVerify(provider),
        "KeyPair failed sign-and-verify challenge."
    );

    const SignatureBundle bundle =
        keyPair.sign(
            "nodo-keypair-message",
            kTestTimestamp,
            provider
        );

    requireCondition(
        !bundle.signatures().empty(),
        "KeyPair did not produce a signature."
    );

    requireCondition(
        provider.verify(
            "nodo-keypair-message",
            bundle.signatures().front()
        ).success(),
        "Provider failed to verify KeyPair signature."
    );

    requireCondition(
        !provider.verify(
            "nodo-keypair-message-tampered",
            bundle.signatures().front()
        ).success(),
        "Provider accepted KeyPair signature for the wrong message."
    );
}

void testPublicIdentityDoesNotExposePrivateKeyMaterial() {
    const KeyPair keyPair =
        KeyPair::createDevelopmentKeyPair("public-identity-check");

    const std::string identity =
        keyPair.publicIdentity();

    const std::string publicSerialization =
        keyPair.serializePublic();

    const std::string privateMaterial =
        keyPair.privateKeyForSigningOnly().keyMaterialForSigningOnly();

    requireCondition(
        identity.find(privateMaterial) == std::string::npos,
        "KeyPair public identity leaked private key material."
    );

    requireCondition(
        publicSerialization.find(privateMaterial) == std::string::npos,
        "KeyPair public serialization leaked private key material."
    );
}

void testInvalidKeyPairIsRejected() {
    const KeyPair invalid;

    requireCondition(
        !invalid.isValid(),
        "Default KeyPair should be invalid."
    );

    bool addressRejected = false;

    try {
        (void)invalid.address();
    } catch (const std::exception&) {
        addressRejected = true;
    }

    requireCondition(
        addressRejected,
        "Invalid KeyPair returned an address."
    );
}

void testMismatchedAlgorithmsAreRejected() {
    const KeyPair mismatched(
        PublicKey(
            CryptoAlgorithm::CLASSIC_ED25519,
            "future-ed25519-public-material"
        ),
        PrivateKey(
            CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
            "development-private-material"
        )
    );

    requireCondition(
        !mismatched.isValid(),
        "KeyPair accepted mismatched key algorithms."
    );
}

void testEmptyDevelopmentSeedIsRejected() {
    bool rejected = false;

    try {
        (void)KeyPair::createDevelopmentKeyPair("");
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Development KeyPair accepted an empty seed."
    );
}

} // namespace

int main() {
    try {
        testDevelopmentKeyPairIsDeterministic();
        testDifferentSeedsProduceDifferentAddresses();
        testKeyPairAddressMatchesPublicKeyDerivation();
        testKeyPairCanSignAndVerify();
        testPublicIdentityDoesNotExposePrivateKeyMaterial();
        testInvalidKeyPairIsRejected();
        testMismatchedAlgorithmsAreRejected();
        testEmptyDevelopmentSeedIsRejected();

        std::cout << "Nodo key management tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo key management tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
