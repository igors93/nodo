#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
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
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
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

KeyPair keyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "consensus-validator-key-" + suffix
    );
}

PublicKey publicKey(
    const std::string& suffix
) {
    return keyPair(suffix).publicKey();
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return keyPair(suffix).privateKeyForSigningOnly();
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

    const Bls12381SignatureProvider provider;

    return ValidatorVoteRecord::createVote(
        validatorAddress(key),
        key,
        privateKey(suffix),
        blockIndex,
        blockHash,
        previousHash,
        round,
        ValidatorVoteDecision::PRECOMMIT,
        "NONE",
        timestamp,
        provider
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

    const Bls12381SignatureProvider provider;

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

void testQuorumCertificateRequiresMoreThanTwoThirds() {
    const ValidatorRegistry registry =
        registryWithThreeValidators();

    const std::vector<ValidatorVoteRecord> twoVotes = {
        approveVote("a", 7, "block-hash-consensus-qc", "previous-hash-consensus", 1, kTimestamp + 10),
        approveVote("b", 7, "block-hash-consensus-qc", "previous-hash-consensus", 1, kTimestamp + 11)
    };

    const Bls12381SignatureProvider provider;

    const auto insufficient = QuorumCertificateBuilder::buildFromVotes(
        7, "block-hash-consensus-qc", "previous-hash-consensus", 1,
        twoVotes, registry, CryptoPolicy::developmentPolicy(), provider);
    requireCondition(
        insufficient.status() == QuorumCertificateBuildStatus::NOT_ENOUGH_VALID_VOTES,
        "Exactly two thirds of a divisible total must not certify."
    );

    std::vector<ValidatorVoteRecord> votes = twoVotes;
    votes.push_back(approveVote("c", 7, "block-hash-consensus-qc",
                                "previous-hash-consensus", 1, kTimestamp + 12));

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
        "All three validators should certify with a strict 2/3 threshold."
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
        result.certificate().requiredVotingWeight() ==
            2 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS + 1 &&
        result.certificate().signedVotingWeight() ==
            3 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS &&
        result.certificate().totalVotingWeight() ==
            3 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS &&
        result.certificate().validatorSetRoot() == registry.validatorSetRoot(),
        "A divisible total requires strictly more than two thirds."
    );

    const nodo::consensus::QuorumCertificate forgedLowThreshold(
        7, "block-hash-consensus-qc", "previous-hash-consensus", 1,
        2 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
        3 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
        2 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
        registry.validatorSetRoot(), twoVotes
    );
    requireCondition(
        !forgedLowThreshold.isStructurallyValid() &&
            !forgedLowThreshold.verify(registry,
                                   CryptoPolicy::developmentPolicy(), provider),
        "A certificate with exactly two-thirds quorum metadata must be rejected."
    );
}

void testQuorumCertificateRejectsNoncanonicalHigherMetadata() {
    ValidatorRegistry registry = registryWithThreeValidators();
    const auto fourth = registrationFor(publicKey("d"), 1, kTimestamp + 4);
    requireCondition(registry.registerValidator(fourth).accepted(),
                     "Fourth validator should register.");

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-high", "previous-hash-consensus", 1, kTimestamp + 10),
        approveVote("b", 7, "block-hash-consensus-high", "previous-hash-consensus", 1, kTimestamp + 11),
        approveVote("c", 7, "block-hash-consensus-high", "previous-hash-consensus", 1, kTimestamp + 12),
        approveVote("d", 7, "block-hash-consensus-high", "previous-hash-consensus", 1, kTimestamp + 13)
    };
    const Bls12381SignatureProvider provider;
    const auto built = QuorumCertificateBuilder::buildFromVotes(
        7, "block-hash-consensus-high", "previous-hash-consensus", 1,
        std::vector<ValidatorVoteRecord>(votes.begin(), votes.begin() + 3), registry,
        CryptoPolicy::developmentPolicy(), provider);
    requireCondition(built.certified() &&
                         built.certificate().requiredVotingWeight() == 2'666'667,
                     "Three of four equal weights should satisfy strict quorum.");

    constexpr std::uint64_t allWeight =
        4 * ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS;
    const nodo::consensus::QuorumCertificate forgedHighThreshold(
        7, "block-hash-consensus-high", "previous-hash-consensus", 1,
        allWeight, allWeight, allWeight, registry.validatorSetRoot(), votes);
    requireCondition(!forgedHighThreshold.isStructurallyValid() &&
                         !forgedHighThreshold.verify(
                             registry, CryptoPolicy::developmentPolicy(), provider),
                     "A certificate must not replace the canonical threshold with unanimity.");
}

void testQuorumRejectsDuplicateVoter() {
    const ValidatorRegistry registry =
        registryWithThreeValidators();

    const std::vector<ValidatorVoteRecord> votes = {
        approveVote("a", 7, "block-hash-consensus-dup", "previous-hash-consensus", 1, kTimestamp + 20),
        approveVote("a", 7, "block-hash-consensus-dup", "previous-hash-consensus", 1, kTimestamp + 21)
    };

    const Bls12381SignatureProvider provider;

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

    const Bls12381SignatureProvider provider;

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

    const Bls12381SignatureProvider provider;

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
        testQuorumCertificateRequiresMoreThanTwoThirds();
        testQuorumCertificateRejectsNoncanonicalHigherMetadata();
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
