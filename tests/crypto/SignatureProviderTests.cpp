#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureVerificationResult.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::CryptoSuiteId;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signature;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SignatureVerificationResult;
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

PublicKey developmentPublicKey() {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-signature-provider-public-key"
    );
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("signature-provider-user");
}

PublicKey userPublicKey() {
    return userKeyPair().publicKey();
}

PrivateKey userPrivateKey() {
    return userKeyPair().privateKeyForSigningOnly();
}

void testEd25519ProviderSignsAndVerifies() {
    const Ed25519SignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-signature-provider-message",
            userPublicKey(),
            userPrivateKey(),
            kTestTimestamp,
            SigningDomain::USER_TRANSACTION
        );

    requireCondition(
        signature.isValid(),
        "Ed25519SignatureProvider produced an invalid signature."
    );

    const SignatureVerificationResult result =
        provider.verify(
            "nodo-signature-provider-message",
            signature
        );

    requireCondition(
        result.success(),
        "Ed25519SignatureProvider failed to verify its own signature."
    );
}

void testEd25519ProviderRejectsWrongMessage() {
    const Ed25519SignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-original-message",
            userPublicKey(),
            userPrivateKey(),
            kTestTimestamp,
            SigningDomain::USER_TRANSACTION
        );

    const SignatureVerificationResult result =
        provider.verify(
            "nodo-tampered-message",
            signature
        );

    requireCondition(
        !result.success(),
        "Ed25519SignatureProvider accepted a signature for the wrong message."
    );
}

void testEd25519ProviderRejectsTamperedSignatureHex() {
    const Ed25519SignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-message-for-tamper-test",
            userPublicKey(),
            userPrivateKey(),
            kTestTimestamp,
            SigningDomain::USER_TRANSACTION
        );

    const std::string originalHex = signature.signatureHex();

    const std::string tamperedHex =
        (originalHex.front() == '0' ? "1" : "0") +
        originalHex.substr(1);

    const Signature tamperedSignature(
        signature.suite(),
        signature.domain(),
        signature.algorithm(),
        signature.publicKey(),
        tamperedHex,
        signature.createdAt()
    );

    const SignatureVerificationResult result =
        provider.verify(
            "nodo-message-for-tamper-test",
            tamperedSignature
        );

    requireCondition(
        !result.success(),
        "Ed25519SignatureProvider accepted a tampered signature."
    );
}

void testSignatureBundleUsesProviderBoundary() {
    const Ed25519SignatureProvider provider;
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();

    const SignatureBundle bundle =
        SignatureBundle::createSignature(
            "nodo-bundle-provider-message",
            userPublicKey(),
            userPrivateKey(),
            kTestTimestamp,
            provider,
            SigningDomain::USER_TRANSACTION
        );

    requireCondition(
        bundle.isValidForPolicy(
            policy,
            SecurityContext::USER_TRANSACTION
        ),
        "SignatureBundle failed policy validation."
    );

    requireCondition(
        bundle.verifyForPolicy(
            "nodo-bundle-provider-message",
            policy,
            SecurityContext::USER_TRANSACTION,
            provider
        ),
        "SignatureBundle failed provider verification."
    );
}

void testSignatureBundleRejectsWrongMessage() {
    const Ed25519SignatureProvider provider;
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();

    const SignatureBundle bundle =
        SignatureBundle::createSignature(
            "nodo-correct-bundle-message",
            userPublicKey(),
            userPrivateKey(),
            kTestTimestamp,
            provider,
            SigningDomain::USER_TRANSACTION
        );

    requireCondition(
        !bundle.verifyForPolicy(
            "nodo-wrong-bundle-message",
            policy,
            SecurityContext::USER_TRANSACTION,
            provider
        ),
        "SignatureBundle accepted the wrong message."
    );
}

void testSignatureRejectsUnsafeHex() {
    const Signature invalidSignature(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        developmentPublicKey(),
        "not-a-hex-signature",
        kTestTimestamp
    );

    requireCondition(
        !invalidSignature.isValid(),
        "Signature accepted unsafe non-hex signature material."
    );
}

} // namespace

int main() {
    try {
        testEd25519ProviderSignsAndVerifies();
        testEd25519ProviderRejectsWrongMessage();
        testEd25519ProviderRejectsTamperedSignatureHex();
        testSignatureBundleUsesProviderBoundary();
        testSignatureBundleRejectsWrongMessage();
        testSignatureRejectsUnsafeHex();

        std::cout << "Nodo signature provider tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo signature provider tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
