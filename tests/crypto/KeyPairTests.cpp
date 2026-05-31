#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::Address;
using nodo::crypto::AddressDerivation;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SigningDomain;

constexpr std::int64_t kTestTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testEd25519KeyPairIsDeterministic() {
    const KeyPair first =
        KeyPair::createDeterministicEd25519KeyPair("igor");

    const KeyPair second =
        KeyPair::createDeterministicEd25519KeyPair("igor");

    requireCondition(
        first.isValid(),
        "Ed25519 KeyPair is invalid."
    );

    requireCondition(
        first.publicKey().serialize() == second.publicKey().serialize(),
        "Ed25519 KeyPair public key is not deterministic."
    );

    requireCondition(
        first.address().value() == second.address().value(),
        "Ed25519 KeyPair address is not deterministic."
    );
}

void testDifferentSeedsProduceDifferentAddresses() {
    const KeyPair first =
        KeyPair::createDeterministicEd25519KeyPair("igor");

    const KeyPair second =
        KeyPair::createDeterministicEd25519KeyPair("ana");

    requireCondition(
        first.address().value() != second.address().value(),
        "Different Ed25519 seeds produced the same address."
    );
}

void testKeyPairAddressMatchesPublicKeyDerivation() {
    const KeyPair keyPair =
        KeyPair::createDeterministicEd25519KeyPair("address-match");

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
    const Ed25519SignatureProvider provider;

    const KeyPair keyPair =
        KeyPair::createDeterministicEd25519KeyPair("signing-keypair");

    requireCondition(
        keyPair.canSignAndVerify(provider),
        "KeyPair failed sign-and-verify challenge."
    );

    const SignatureBundle bundle =
        keyPair.sign(
            "nodo-keypair-message",
            kTestTimestamp,
            provider,
            SigningDomain::USER_TRANSACTION
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
        KeyPair::createDeterministicEd25519KeyPair("public-identity-check");

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
    const KeyPair ed25519 =
        KeyPair::createDeterministicEd25519KeyPair("mismatch-ed25519");
    const KeyPair bls =
        KeyPair::createDeterministicBls12381KeyPair("mismatch-bls");

    const KeyPair mismatched(
        ed25519.publicKey(),
        bls.privateKeyForSigningOnly()
    );

    requireCondition(
        !mismatched.isValid(),
        "KeyPair accepted mismatched key algorithms."
    );
}

void testEmptyDeterministicSeedIsRejected() {
    bool rejected = false;

    try {
        (void)KeyPair::createDeterministicEd25519KeyPair("");
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Ed25519 KeyPair accepted an empty seed."
    );
}

} // namespace

int main() {
    try {
        testEd25519KeyPairIsDeterministic();
        testDifferentSeedsProduceDifferentAddresses();
        testKeyPairAddressMatchesPublicKeyDerivation();
        testKeyPairCanSignAndVerify();
        testPublicIdentityDoesNotExposePrivateKeyMaterial();
        testInvalidKeyPairIsRejected();
        testMismatchedAlgorithmsAreRejected();
        testEmptyDeterministicSeedIsRejected();

        std::cout << "Nodo key management tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo key management tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
