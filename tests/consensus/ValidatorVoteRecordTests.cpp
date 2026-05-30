#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/DevelopmentSignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::consensus::QuorumCertificateBuilder;
using nodo::consensus::QuorumCertificateBuildStatus;
using nodo::consensus::ValidatorVoteDecision;
using nodo::consensus::ValidatorVoteRecord;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::crypto::AddressDerivation;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::DevelopmentSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

PublicKey publicKey(
    const std::string& suffix
) {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "consensus-validator-public-key-" + suffix
    );
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "consensus-validator-private-key-" + suffix
    );
}

std::string validatorAddress(
    const PublicKey& key
) {
    return AddressDerivation::deriveFromPublicKey(key).value();
}

ValidatorRegistrationRecord registrationFor(
    const PublicKey& key,
    std::uint64_t activationEpoch,
    std::int64_t timestamp
) {
    return ValidatorRegistrationRecord(
        validatorAddress(key),
        key,
        activationEpoch,
        "consensus-validator-metadata-" + key.fingerprint().substr(0, 12),
        timestamp
    );
}

ValidatorVoteRecord approveVote(
    const std::string& suffix,
    std::uint64_t blockIndex = 7,
    const std::string& blockHash = "block-hash-consensus-a",
    const std::string& previousHash = "previous-hash-consensus",
    std::uint64_t round = 1,
    std::int64_t timestamp = kTimestamp
) {
    const PublicKey key =
        publicKey(suffix);

    return ValidatorVoteRecord::createDevelopmentVote(
        validatorAddress(key),
        key,
        privateKey(suffix),
        blockIndex,
        blockHash,
        previousHash,
        round,
        ValidatorVoteDecision::APPROVE,
        "NONE",
        timestamp
    );
}

ValidatorRegistry registryWithThreeValidators() {
    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(
            registrationFor(publicKey("a"), 1, kTimestamp + 1)
        ).accepted(),
        "Validator A registration should be accepted."
    );

    requireCondition(
        registry.registerValidator(
            registrationFor(publicKey("b"), 1, kTimestamp + 2)
        ).accepted(),
        "Validator B registration should be accepted."
    );

    requireCondition(
        registry.registerValidator(
            registrationFor(publicKey("c"), 1, kTimestamp + 3)
        ).accepted(),
        "Validator C registration should be accepted."
    );

    return registry;
}

void testVoteSignsAndVerifies() {
    const ValidatorVoteRecord vote =
        approveVote("a");

    const DevelopmentSignatureProvider provider;

    requireCondition(
        vote.isStructurallyValid(CryptoPolicy::developmentPolicy()),
        "Vote should be structurally valid."
    );

    requireCondition(
        vote.verify(
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Vote signature should verify."
    );

    requireCondition(
        vote.matchesBlock(
            7,
            "block-hash-consensus-a",
            1
        ),
        "Vote should match its target block."
    );
}

void testQuorumCertificateBuildsWithTwoOfThree() {
    const ValidatorRegistry registry =
        registryWithThreeValidators();

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-qc", "previous-hash-consensus", 1, kTimestamp + 10),
        approveVote("b", 7, "block-hash-consensus-qc", "previous-hash-consensus", 1, kTimestamp + 11)
    };

    const DevelopmentSignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            7,
            "block-hash-consensus-qc",
            "previous-hash-consensus",
            1,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        result.certified(),
        "Two of three active validators should certify with 2/3 threshold."
    );

    requireCondition(
        result.certificate().verify(
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Built quorum certificate should verify."
    );

    requireCondition(
        result.certificate().requiredVoteCount() == 2U,
        "Required vote count for 2 of 3 threshold should be two."
    );
}

void testQuorumRejectsDuplicateVoter() {
    const ValidatorRegistry registry =
        registryWithThreeValidators();

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-dup", "previous-hash-consensus", 1, kTimestamp + 20),
        approveVote("a", 7, "block-hash-consensus-dup", "previous-hash-consensus", 1, kTimestamp + 21)
    };

    const DevelopmentSignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            7,
            "block-hash-consensus-dup",
            "previous-hash-consensus",
            1,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        result.status() == QuorumCertificateBuildStatus::DUPLICATE_VOTER,
        "Duplicate voter should be rejected."
    );
}

void testQuorumRejectsUnregisteredVoter() {
    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(
            registrationFor(publicKey("a"), 1, kTimestamp + 30)
        ).accepted(),
        "Validator A registration should be accepted."
    );

    requireCondition(
        registry.registerValidator(
            registrationFor(publicKey("b"), 1, kTimestamp + 31)
        ).accepted(),
        "Validator B registration should be accepted."
    );

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-unregistered", "previous-hash-consensus", 1, kTimestamp + 32),
        approveVote("x-unregistered", 7, "block-hash-consensus-unregistered", "previous-hash-consensus", 1, kTimestamp + 33)
    };

    const DevelopmentSignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            7,
            "block-hash-consensus-unregistered",
            "previous-hash-consensus",
            1,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        result.status() == QuorumCertificateBuildStatus::UNREGISTERED_OR_INACTIVE_VOTER,
        "Unregistered voter should be rejected."
    );
}

void testQuorumRejectsConflictingBlockVote() {
    const ValidatorRegistry registry =
        registryWithThreeValidators();

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-target", "previous-hash-consensus", 1, kTimestamp + 40),
        approveVote("b", 7, "block-hash-consensus-other", "previous-hash-consensus", 1, kTimestamp + 41)
    };

    const DevelopmentSignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            7,
            "block-hash-consensus-target",
            "previous-hash-consensus",
            1,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        result.status() == QuorumCertificateBuildStatus::CONFLICTING_VOTE,
        "Vote for different block hash should be rejected."
    );
}

} // namespace

int main() {
    try {
        testVoteSignsAndVerifies();
        testQuorumCertificateBuildsWithTwoOfThree();
        testQuorumRejectsDuplicateVoter();
        testQuorumRejectsUnregisteredVoter();
        testQuorumRejectsConflictingBlockVote();

        std::cout << "Nodo validator vote and quorum certificate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator vote and quorum certificate tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
