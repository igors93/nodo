#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ProtectionBlockProposal.hpp"
#include "core/ValidatorBlockProposalSignature.hpp"
#include "core/ValidatorProposalRegistry.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/DevelopmentSignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardDistributor.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::LedgerRecord;
using nodo::core::ProtectionBlockBuilder;
using nodo::core::ProtectionBlockProposal;
using nodo::core::SignedProtectionBlockProposal;
using nodo::core::ValidatorBlockProposalSigner;
using nodo::core::ValidatorProposalRegistrationStatus;
using nodo::core::ValidatorProposalRegistry;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::DevelopmentSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::economics::EpochEmissionPolicy;
using nodo::economics::EpochRewardDistribution;
using nodo::economics::EpochRewardDistributor;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::economics::ValidatorScoreReason;
using nodo::economics::ValidatorScoreRecord;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

ValidationWorkRecord work(
    const std::string& validator,
    std::uint64_t epoch,
    std::uint32_t weight,
    const std::string& evidenceHash
) {
    return ValidationWorkRecord(
        validator,
        epoch,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "target-" + evidenceHash,
        evidenceHash,
        weight,
        kTimestamp
    );
}

ValidatorScoreRecord score(
    const std::string& validator,
    std::uint64_t epoch,
    std::int32_t newScore
) {
    return ValidatorScoreRecord(
        validator,
        epoch,
        50,
        newScore,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence-" + validator,
        kTimestamp
    );
}

Blockchain baseBlockchain(std::int64_t timestampOffset = 0) {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    work(
                        "nodo1bootstrap",
                        1,
                        1,
                        "registry-bootstrap-evidence-" + std::to_string(timestampOffset)
                    ),
                    kTimestamp + timestampOffset
                )
            },
            kTimestamp + timestampOffset + 1
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);
    return blockchain;
}

EpochRewardDistribution rewardDistribution(
    std::uint64_t epochId,
    const std::string& acceptedBlockHash
) {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", epochId, 60, "registry-evidence-a-" + acceptedBlockHash),
        work("nodo1validatorB", epochId, 40, "registry-evidence-b-" + acceptedBlockHash)
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", epochId, 100),
        score("nodo1validatorB", epochId, 50)
    };

    return EpochRewardDistributor::distribute(
        epochId,
        10,
        20,
        Amount::fromNodo(36500),
        Amount::fromNodo(3),
        100,
        policy,
        workRecords,
        scoreRecords,
        acceptedBlockHash,
        kTimestamp + 10
    );
}

PublicKey validatorPublicKey(const std::string& validator) {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "registry-public-key-" + validator
    );
}

PrivateKey validatorPrivateKey(const std::string& validator) {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "registry-private-key-" + validator
    );
}

ProtectionBlockProposal proposalFor(
    const Blockchain& blockchain,
    const std::string& acceptedBlockHash,
    std::int64_t blockTimestamp
) {
    return ProtectionBlockBuilder::buildRewardBlockProposal(
        blockchain,
        rewardDistribution(1, acceptedBlockHash),
        blockTimestamp
    );
}

SignedProtectionBlockProposal signedProposalFor(
    const Blockchain& blockchain,
    const std::string& validator,
    const std::string& acceptedBlockHash,
    std::int64_t blockTimestamp,
    std::int64_t signatureTimestamp
) {
    const ProtectionBlockProposal proposal =
        proposalFor(
            blockchain,
            acceptedBlockHash,
            blockTimestamp
        );

    return ValidatorBlockProposalSigner::signProposalForDevelopment(
        proposal,
        validator,
        validatorPublicKey(validator),
        validatorPrivateKey(validator),
        signatureTimestamp
    );
}

void testRegistryAcceptsValidSignedProposal() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-block-a",
            kTimestamp + 30,
            kTimestamp + 31
        );

    const auto result =
        registry.registerSignedProposal(
            signedProposal,
            blockchain,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(result.accepted(), "Valid signed proposal should be accepted.");
    requireCondition(registry.entries().size() == 1U, "Registry should contain one proposal.");
    requireCondition(registry.conflicts().empty(), "Registry should have no conflicts.");
    requireCondition(registry.isValid(), "Registry should remain valid.");
}

void testRegistryTreatsSameProposalAsDuplicate() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-duplicate-block",
            kTimestamp + 40,
            kTimestamp + 41
        );

    const auto first = registry.registerSignedProposal(
        signedProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    const auto second = registry.registerSignedProposal(
        signedProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(first.accepted(), "First proposal should be accepted.");
    requireCondition(second.duplicate(), "Second identical proposal should be duplicate.");
    requireCondition(registry.entries().size() == 1U, "Duplicate should not add a new registry entry.");
    requireCondition(registry.conflicts().empty(), "Duplicate should not create conflict evidence.");
}

void testRegistryDetectsDoubleSignForSameValidatorAndHeight() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal firstProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-conflict-a",
            kTimestamp + 50,
            kTimestamp + 51
        );

    const SignedProtectionBlockProposal conflictingProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-conflict-b",
            kTimestamp + 52,
            kTimestamp + 53
        );

    const auto first = registry.registerSignedProposal(
        firstProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    const auto second = registry.registerSignedProposal(
        conflictingProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(first.accepted(), "First proposal should be accepted.");
    requireCondition(second.conflictDetected(), "Second conflicting proposal should be flagged.");
    requireCondition(second.status() == ValidatorProposalRegistrationStatus::DOUBLE_SIGN_CONFLICT, "Wrong conflict status.");
    requireCondition(registry.entries().size() == 1U, "Conflicting proposal should not be accepted as normal entry.");
    requireCondition(registry.conflicts().size() == 1U, "Conflict evidence should be recorded.");
    requireCondition(registry.hasDoubleSignConflict("nodo1validatorA", firstProposal.proposal().block().index()), "Registry should report validator conflict.");
    requireCondition(registry.conflictCountForValidator("nodo1validatorA") == 1U, "Conflict count mismatch.");
}

void testDifferentValidatorsCanRegisterCompetingProposals() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal firstProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-competing-a",
            kTimestamp + 60,
            kTimestamp + 61
        );

    const SignedProtectionBlockProposal secondProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorB",
            "registry-competing-b",
            kTimestamp + 62,
            kTimestamp + 63
        );

    const auto first = registry.registerSignedProposal(
        firstProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    const auto second = registry.registerSignedProposal(
        secondProposal,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(first.accepted(), "First validator proposal should be accepted.");
    requireCondition(second.accepted(), "Different validator proposal should be accepted.");
    requireCondition(registry.entries().size() == 2U, "Registry should contain both validator proposals.");
    requireCondition(registry.conflicts().empty(), "Different validators should not create double-sign evidence.");
}

void testRegistryRejectsInvalidSignatureBeforeConflictCheck() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal original =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-valid-original",
            kTimestamp + 70,
            kTimestamp + 71
        );

    const ProtectionBlockProposal otherProposal =
        proposalFor(
            blockchain,
            "registry-invalid-signature-target",
            kTimestamp + 72
        );

    const SignedProtectionBlockProposal tampered(
        otherProposal,
        original.proposalSignature()
    );

    const auto validResult = registry.registerSignedProposal(
        original,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    const auto invalidResult = registry.registerSignedProposal(
        tampered,
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(validResult.accepted(), "Original proposal should be accepted.");
    requireCondition(invalidResult.status() == ValidatorProposalRegistrationStatus::INVALID_SIGNATURE, "Tampered proposal should fail signature verification.");
    requireCondition(registry.conflicts().empty(), "Invalid signature should not create conflict evidence.");
}

void testRegistryRejectsProposalForDifferentChainTip() {
    Blockchain originalChain = baseBlockchain(0);
    Blockchain otherChain = baseBlockchain(100);
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            originalChain,
            "nodo1validatorA",
            "registry-wrong-tip",
            kTimestamp + 80,
            kTimestamp + 81
        );

    const auto result = registry.registerSignedProposal(
        signedProposal,
        otherChain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(result.status() == ValidatorProposalRegistrationStatus::INVALID_PROPOSAL, "Wrong chain tip should be invalid proposal.");
    requireCondition(registry.entries().empty(), "Wrong-tip proposal should not be stored.");
}

void testRegistryRejectsDevelopmentSignatureUnderProductionLikePolicy() {
    Blockchain blockchain = baseBlockchain();
    ValidatorProposalRegistry registry;
    const DevelopmentSignatureProvider provider;

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            blockchain,
            "nodo1validatorA",
            "registry-production-policy",
            kTimestamp + 90,
            kTimestamp + 91
        );

    const auto result = registry.registerSignedProposal(
        signedProposal,
        blockchain,
        CryptoPolicy::futureHybridPolicy(),
        provider
    );

    requireCondition(result.status() == ValidatorProposalRegistrationStatus::INVALID_SIGNATURE, "Production-like policy should reject development signature.");
    requireCondition(registry.entries().empty(), "Rejected signature should not be stored.");
}

} // namespace

int main() {
    try {
        testRegistryAcceptsValidSignedProposal();
        testRegistryTreatsSameProposalAsDuplicate();
        testRegistryDetectsDoubleSignForSameValidatorAndHeight();
        testDifferentValidatorsCanRegisterCompetingProposals();
        testRegistryRejectsInvalidSignatureBeforeConflictCheck();
        testRegistryRejectsProposalForDifferentChainTip();
        testRegistryRejectsDevelopmentSignatureUnderProductionLikePolicy();

        std::cout << "Nodo validator proposal registry tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator proposal registry tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
