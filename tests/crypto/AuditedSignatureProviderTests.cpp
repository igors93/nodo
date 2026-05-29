#include "crypto/AuditedSignatureProvider.hpp"
#include "crypto/AuditedSignatureProviderProfile.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureProviderRegistry.hpp"
#include "crypto/SignatureVerificationResult.hpp"
#include "crypto/hash.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::AuditedSignatureProvider;
using nodo::crypto::AuditedSignatureProviderProfile;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::Signature;
using nodo::crypto::SignatureProviderRegistry;
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

std::string testOnlySignatureHex(
    const std::string& message,
    const PublicKey& publicKey
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        "NODO_TEST_ONLY_AUDITED_PROVIDER_V1|" +
        message +
        "|" +
        publicKey.serialize();

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

/*
 * Test-only audited provider adapter.
 *
 * This class exists only inside tests. It proves that the audited provider
 * boundary can be implemented and registered. It is not production crypto.
 */
class TestOnlyAuditedEd25519Provider final : public AuditedSignatureProvider {
public:
    explicit TestOnlyAuditedEd25519Provider(
        bool productionReady
    )
        : m_productionReady(productionReady) {}

    CryptoAlgorithm algorithm() const override {
        return CryptoAlgorithm::CLASSIC_ED25519;
    }

    AuditedSignatureProviderProfile providerProfile() const override {
        return AuditedSignatureProviderProfile(
            CryptoAlgorithm::CLASSIC_ED25519,
            "test-only-ed25519-provider",
            "test-v1",
            m_productionReady ? "TEST-AUDIT-REFERENCE" : "UNVERIFIED",
            m_productionReady ? "TEST-IMPLEMENTATION-REFERENCE" : "UNVERIFIED",
            m_productionReady
        );
    }

    Signature sign(
        const std::string& message,
        const PublicKey& publicKey,
        const PrivateKey& privateKey,
        std::int64_t timestamp
    ) const override {
        if (message.empty()) {
            throw std::invalid_argument("Test provider message cannot be empty.");
        }

        if (timestamp <= 0) {
            throw std::invalid_argument("Test provider timestamp must be positive.");
        }

        if (publicKey.algorithm() != algorithm()) {
            throw std::invalid_argument("Test provider public key algorithm mismatch.");
        }

        if (privateKey.algorithm() != algorithm()) {
            throw std::invalid_argument("Test provider private key algorithm mismatch.");
        }

        if (!publicKey.isValid() || !privateKey.isValid()) {
            throw std::invalid_argument("Test provider received invalid keys.");
        }

        return Signature(
            algorithm(),
            publicKey,
            testOnlySignatureHex(message, publicKey),
            timestamp
        );
    }

    SignatureVerificationResult verify(
        const std::string& message,
        const Signature& signature
    ) const override {
        if (message.empty()) {
            return SignatureVerificationResult::invalid("Message is empty.");
        }

        if (!signature.isValid()) {
            return SignatureVerificationResult::invalid("Signature is structurally invalid.");
        }

        if (signature.algorithm() != algorithm()) {
            return SignatureVerificationResult::invalid("Algorithm mismatch.");
        }

        const std::string expected =
            testOnlySignatureHex(
                message,
                signature.publicKey()
            );

        if (signature.signatureHex() != expected) {
            return SignatureVerificationResult::invalid("Signature mismatch.");
        }

        return SignatureVerificationResult::valid();
    }

private:
    bool m_productionReady;
};

PublicKey classicPublicKey() {
    return PublicKey(
        CryptoAlgorithm::CLASSIC_ED25519,
        "test-classic-ed25519-public-key-material"
    );
}

PrivateKey classicPrivateKey() {
    return PrivateKey(
        CryptoAlgorithm::CLASSIC_ED25519,
        "test-classic-ed25519-private-key-material"
    );
}

void testAuditedProfileValidation() {
    const AuditedSignatureProviderProfile profile(
        CryptoAlgorithm::CLASSIC_ED25519,
        "ed25519-provider",
        "v1",
        "AUDIT-REFERENCE-001",
        "IMPLEMENTATION-REFERENCE-001",
        true
    );

    requireCondition(
        profile.isValid(),
        "Audited provider profile should be valid."
    );

    requireCondition(
        profile.allowsProductionUse(),
        "Audited provider profile should allow production use."
    );

    requireCondition(
        profile.serialize().find("CLASSIC_ED25519") != std::string::npos,
        "Audited provider profile serialization is missing algorithm."
    );
}

void testDevelopmentAlgorithmIsRejected() {
    const AuditedSignatureProviderProfile profile(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "development-provider",
        "v1",
        "AUDIT-REFERENCE",
        "IMPLEMENTATION-REFERENCE",
        true
    );

    requireCondition(
        !profile.isValid(),
        "Development algorithm was accepted as audited provider."
    );

    requireCondition(
        !profile.allowsProductionUse(),
        "Development algorithm was allowed for production use."
    );
}

void testUnverifiedProfileIsNotProductionReady() {
    const AuditedSignatureProviderProfile profile(
        CryptoAlgorithm::CLASSIC_ED25519,
        "ed25519-provider",
        "v1",
        "UNVERIFIED",
        "UNVERIFIED",
        true
    );

    requireCondition(
        profile.isValid(),
        "Unverified placeholder profile should still be structurally valid."
    );

    requireCondition(
        !profile.allowsProductionUse(),
        "Unverified placeholder profile was allowed for production use."
    );
}

void testRegistryAcceptsAuditedProvider() {
    SignatureProviderRegistry registry;

    registry.registerAuditedProvider(
        std::make_shared<TestOnlyAuditedEd25519Provider>(true)
    );

    requireCondition(
        registry.size() == 1U,
        "Audited provider registry size is wrong."
    );

    requireCondition(
        registry.hasAuditedProvider(CryptoAlgorithm::CLASSIC_ED25519),
        "Audited provider registry did not find Ed25519 provider."
    );

    requireCondition(
        registry.hasProductionReadyProvider(CryptoAlgorithm::CLASSIC_ED25519),
        "Audited provider registry did not mark provider production ready."
    );
}

void testRegistryRejectsDuplicateProvider() {
    SignatureProviderRegistry registry;

    registry.registerAuditedProvider(
        std::make_shared<TestOnlyAuditedEd25519Provider>(true)
    );

    bool rejected = false;

    try {
        registry.registerAuditedProvider(
            std::make_shared<TestOnlyAuditedEd25519Provider>(true)
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Audited provider registry accepted duplicate algorithm."
    );
}

void testProviderSignsAndVerifies() {
    const TestOnlyAuditedEd25519Provider provider(true);

    const Signature signature =
        provider.sign(
            "nodo-audited-provider-message",
            classicPublicKey(),
            classicPrivateKey(),
            kTestTimestamp
        );

    requireCondition(
        signature.isValid(),
        "Test-only audited provider produced invalid signature."
    );

    requireCondition(
        provider.verify(
            "nodo-audited-provider-message",
            signature
        ).success(),
        "Test-only audited provider failed to verify matching message."
    );

    requireCondition(
        !provider.verify(
            "nodo-audited-provider-message-tampered",
            signature
        ).success(),
        "Test-only audited provider accepted wrong message."
    );
}

void testProviderReadinessGate() {
    const TestOnlyAuditedEd25519Provider verifiedProvider(true);
    const TestOnlyAuditedEd25519Provider unverifiedProvider(false);

    requireCondition(
        verifiedProvider.isReadyForProductionUse(),
        "Verified test provider should pass readiness gate."
    );

    requireCondition(
        !unverifiedProvider.isReadyForProductionUse(),
        "Unverified test provider should fail readiness gate."
    );
}

} // namespace

int main() {
    try {
        testAuditedProfileValidation();
        testDevelopmentAlgorithmIsRejected();
        testUnverifiedProfileIsNotProductionReady();
        testRegistryAcceptsAuditedProvider();
        testRegistryRejectsDuplicateProvider();
        testProviderSignsAndVerifies();
        testProviderReadinessGate();

        std::cout << "Nodo audited signature provider tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo audited signature provider tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
