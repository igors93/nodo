#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PostQuantumAlgorithmProfile.hpp"
#include "crypto/PostQuantumMigrationPlan.hpp"
#include "crypto/PostQuantumSignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureVerificationResult.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::PostQuantumAlgorithmProfile;
using nodo::crypto::PostQuantumMigrationPlan;
using nodo::crypto::PostQuantumSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::Signature;
using nodo::crypto::SignatureVerificationResult;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

/*
 * Test-only provider proving that the post-quantum interface can be
 * implemented without changing blockchain code.
 *
 * It intentionally does not produce real signatures.
 */
class InterfaceOnlyMlDsaProvider final : public PostQuantumSignatureProvider {
public:
    CryptoAlgorithm algorithm() const override {
        return CryptoAlgorithm::POST_QUANTUM_ML_DSA;
    }

    std::string providerName() const override {
        return "interface-only-ml-dsa-provider";
    }

    std::string algorithmFamily() const override {
        return "ML-DSA";
    }

    std::uint32_t claimedSecurityLevel() const override {
        return 3U;
    }

    std::uint32_t publicKeySizeBytes() const override {
        return 0U;
    }

    std::uint32_t privateKeySizeBytes() const override {
        return 0U;
    }

    std::uint32_t signatureSizeBytes() const override {
        return 0U;
    }

    bool isProductionReady() const override {
        return false;
    }

    Signature sign(
        const std::string&,
        const PublicKey&,
        const PrivateKey&,
        std::int64_t
    ) const override {
        throw std::logic_error("Interface-only ML-DSA provider cannot sign.");
    }

    SignatureVerificationResult verify(
        const std::string&,
        const Signature&
    ) const override {
        return SignatureVerificationResult::invalid(
            "Interface-only ML-DSA provider cannot verify."
        );
    }
};

void testKnownPostQuantumProfilesAreValid() {
    const std::vector<PostQuantumAlgorithmProfile> profiles =
        PostQuantumAlgorithmProfile::knownProfiles();

    requireCondition(
        profiles.size() == 2U,
        "Unexpected number of known post-quantum profiles."
    );

    for (const auto& profile : profiles) {
        requireCondition(
            profile.isValid(),
            "Known post-quantum profile is invalid."
        );

        requireCondition(
            !profile.productionReady(),
            "Post-quantum profile incorrectly claims production readiness."
        );

        requireCondition(
            profile.implementationStatus() == "INTERFACE_ONLY_NO_AUDITED_PROVIDER",
            "Post-quantum profile has unexpected implementation status."
        );
    }
}

void testMlDsaProfile() {
    const PostQuantumAlgorithmProfile profile =
        PostQuantumAlgorithmProfile::profileForAlgorithm(
            CryptoAlgorithm::POST_QUANTUM_ML_DSA
        );

    requireCondition(
        profile.familyName() == "ML-DSA",
        "ML-DSA profile has wrong family name."
    );

    requireCondition(
        profile.claimedSecurityLevel() == 3U,
        "ML-DSA profile has wrong claimed security level."
    );

    requireCondition(
        profile.serialize().find("POST_QUANTUM_ML_DSA") != std::string::npos,
        "ML-DSA profile serialization does not include algorithm."
    );
}

void testSlhDsaProfile() {
    const PostQuantumAlgorithmProfile profile =
        PostQuantumAlgorithmProfile::profileForAlgorithm(
            CryptoAlgorithm::POST_QUANTUM_SLH_DSA
        );

    requireCondition(
        profile.familyName() == "SLH-DSA",
        "SLH-DSA profile has wrong family name."
    );

    requireCondition(
        profile.claimedSecurityLevel() == 3U,
        "SLH-DSA profile has wrong claimed security level."
    );

    requireCondition(
        profile.serialize().find("POST_QUANTUM_SLH_DSA") != std::string::npos,
        "SLH-DSA profile serialization does not include algorithm."
    );
}

void testNonPostQuantumAlgorithmsAreRejected() {
    requireCondition(
        !PostQuantumAlgorithmProfile::isPostQuantumProviderCandidate(
            CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE
        ),
        "Development fake signature was accepted as post-quantum candidate."
    );

    requireCondition(
        !PostQuantumAlgorithmProfile::isPostQuantumProviderCandidate(
            CryptoAlgorithm::CLASSIC_ED25519
        ),
        "Classic Ed25519 was accepted as post-quantum candidate."
    );

    bool rejected = false;

    try {
        (void)PostQuantumAlgorithmProfile::profileForAlgorithm(
            CryptoAlgorithm::CLASSIC_ECDSA_SECP256K1
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Classic algorithm profile lookup was not rejected."
    );
}

void testPostQuantumMigrationPlan() {
    const PostQuantumMigrationPlan plan =
        PostQuantumMigrationPlan::developmentHybridPlan();

    requireCondition(
        plan.isValid(),
        "Development post-quantum migration plan is invalid."
    );

    requireCondition(
        plan.classicAlgorithm() == CryptoAlgorithm::CLASSIC_ED25519,
        "Migration plan classic algorithm is unexpected."
    );

    requireCondition(
        plan.postQuantumAlgorithm() == CryptoAlgorithm::POST_QUANTUM_ML_DSA,
        "Migration plan post-quantum algorithm is unexpected."
    );

    requireCondition(
        plan.serialize().find("NODO_PQ_MIGRATION_PLAN_V1") != std::string::npos,
        "Migration plan serialization does not include version."
    );
}

void testInterfaceOnlyProviderShape() {
    const InterfaceOnlyMlDsaProvider provider;

    requireCondition(
        provider.algorithm() == CryptoAlgorithm::POST_QUANTUM_ML_DSA,
        "Interface-only provider has wrong algorithm."
    );

    requireCondition(
        provider.algorithmFamily() == "ML-DSA",
        "Interface-only provider has wrong family."
    );

    requireCondition(
        !provider.isProductionReady(),
        "Interface-only provider incorrectly claims production readiness."
    );

    const SignatureVerificationResult result =
        provider.verify(
            "message",
            Signature()
        );

    requireCondition(
        !result.success(),
        "Interface-only provider accepted verification."
    );
}

} // namespace

int main() {
    try {
        testKnownPostQuantumProfilesAreValid();
        testMlDsaProfile();
        testSlhDsaProfile();
        testNonPostQuantumAlgorithmsAreRejected();
        testPostQuantumMigrationPlan();
        testInterfaceOnlyProviderShape();

        std::cout << "Nodo post-quantum provider interface tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo post-quantum provider interface tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}