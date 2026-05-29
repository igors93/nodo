#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/DevelopmentSignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureVerificationResult.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::DevelopmentSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signature;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SignatureVerificationResult;

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

PrivateKey developmentPrivateKey() {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "igor-signature-provider-private-key"
    );
}

void testDevelopmentProviderSignsAndVerifies() {
    const DevelopmentSignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-signature-provider-message",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp
        );

    requireCondition(
        signature.isValid(),
        "DevelopmentSignatureProvider produced an invalid signature."
    );

    const SignatureVerificationResult result =
        provider.verify(
            "nodo-signature-provider-message",
            signature
        );

    requireCondition(
        result.success(),
        "DevelopmentSignatureProvider failed to verify its own signature."
    );
}

void testDevelopmentProviderRejectsWrongMessage() {
    const DevelopmentSignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-original-message",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp
        );

    const SignatureVerificationResult result =
        provider.verify(
            "nodo-tampered-message",
            signature
        );

    requireCondition(
        !result.success(),
        "DevelopmentSignatureProvider accepted a signature for the wrong message."
    );
}

void testDevelopmentProviderRejectsTamperedSignatureHex() {
    const DevelopmentSignatureProvider provider;

    const Signature signature =
        provider.sign(
            "nodo-message-for-tamper-test",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp
        );

    const std::string originalHex = signature.signatureHex();

    const std::string tamperedHex =
        (originalHex.front() == '0' ? "1" : "0") +
        originalHex.substr(1);

    const Signature tamperedSignature(
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
        "DevelopmentSignatureProvider accepted a tampered signature."
    );
}

void testSignatureBundleUsesProviderBoundary() {
    const DevelopmentSignatureProvider provider;
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();

    const SignatureBundle bundle =
        SignatureBundle::createSignature(
            "nodo-bundle-provider-message",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp,
            provider
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
    const DevelopmentSignatureProvider provider;
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();

    const SignatureBundle bundle =
        SignatureBundle::createDevelopmentSignature(
            "nodo-correct-bundle-message",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp
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

void testProductionPolicyRejectsDevelopmentSignature() {
    const DevelopmentSignatureProvider provider;
    const CryptoPolicy policy = CryptoPolicy::futureHybridPolicy();

    const SignatureBundle bundle =
        SignatureBundle::createDevelopmentSignature(
            "nodo-production-policy-message",
            developmentPublicKey(),
            developmentPrivateKey(),
            kTestTimestamp
        );

    requireCondition(
        !bundle.isValidForPolicy(
            policy,
            SecurityContext::USER_TRANSACTION
        ),
        "Production policy accepted a development-only signature."
    );

    requireCondition(
        !bundle.verifyForPolicy(
            "nodo-production-policy-message",
            policy,
            SecurityContext::USER_TRANSACTION,
            provider
        ),
        "Production policy verified a development-only signature."
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
        testDevelopmentProviderSignsAndVerifies();
        testDevelopmentProviderRejectsWrongMessage();
        testDevelopmentProviderRejectsTamperedSignatureHex();
        testSignatureBundleUsesProviderBoundary();
        testSignatureBundleRejectsWrongMessage();
        testProductionPolicyRejectsDevelopmentSignature();
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
