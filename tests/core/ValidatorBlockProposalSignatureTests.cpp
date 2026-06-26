#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ProtectionBlockProposal.hpp"
#include "core/ValidatorBlockProposalSignature.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
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
using nodo::core::ValidatorBlockProposalSignature;
using nodo::core::ValidatorBlockProposalSigner;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
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

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
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

Blockchain baseBlockchain(
    std::int64_t timestampOffset = 0
) {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    work(
                        "nodo1bootstrap",
                        1,
                        1,
                        "signed-bootstrap-evidence-" + std::to_string(timestampOffset)
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
    std::uint64_t epochId = 1
) {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", epochId, 60, "signed-proposal-evidence-a"),
        work("nodo1validatorB", epochId, 40, "signed-proposal-evidence-b")
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
        "accepted-block-hash-for-signed-proposal",
        kTimestamp + 10
    );
}

KeyPair validatorKeyPair(
    const std::string& suffix = "default"
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "validator-proposal-key-" + suffix
    );
}

PublicKey validatorPublicKey(
    const std::string& suffix = "default"
) {
    return validatorKeyPair(suffix).publicKey();
}

PrivateKey validatorPrivateKey(
    const std::string& suffix = "default"
) {
    return validatorKeyPair(suffix).privateKeyForSigningOnly();
}

ProtectionBlockProposal proposalFor(
    const Blockchain& blockchain,
    std::int64_t timestamp = kTimestamp + 30
) {
    return ProtectionBlockBuilder::buildRewardBlockProposal(
        blockchain,
        rewardDistribution(),
        timestamp
    );
}

void testValidatorSignsAndVerifiesProposal() {
    Blockchain blockchain =
        baseBlockchain();

    const ProtectionBlockProposal proposal =
        proposalFor(blockchain);

    const SignedProtectionBlockProposal signedProposal =
        ValidatorBlockProposalSigner::signProposal(
            proposal,
            "nodo1validatorA",
            validatorPublicKey(),
            validatorPrivateKey(),
            kTimestamp + 31,
            Bls12381SignatureProvider()
        );

    const Bls12381SignatureProvider provider;

    requireCondition(
        signedProposal.isValidForBlockchain(
            blockchain,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Signed proposal should verify for current blockchain tip."
    );

    requireCondition(
        signedProposal.proposalSignature().matchesProposal(proposal),
        "Proposal signature should be bound to the exact proposal."
    );

    requireCondition(
        signedProposal.proposalSignature().signedBlockHash() == proposal.block().hash(),
        "Signed block hash mismatch."
    );

    requireCondition(
        signedProposal.proposalSignature().validatorPublicKey().fingerprint() ==
            validatorPublicKey().fingerprint(),
        "Validator public key fingerprint mismatch."
    );

    signedProposal.appendToBlockchain(
        blockchain,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(
        blockchain.size() == 2U,
        "Signed proposal append should increase chain size."
    );

    requireCondition(
        blockchain.isValid(false),
        "Blockchain should remain valid after signed proposal append."
    );
}

void testSignatureCannotBeReusedOnDifferentProposal() {
    Blockchain blockchain =
        baseBlockchain();

    const ProtectionBlockProposal originalProposal =
        proposalFor(blockchain, kTimestamp + 40);

    const ProtectionBlockProposal otherProposal =
        proposalFor(blockchain, kTimestamp + 41);

    const SignedProtectionBlockProposal signedOriginal =
        ValidatorBlockProposalSigner::signProposal(
            originalProposal,
            "nodo1validatorA",
            validatorPublicKey(),
            validatorPrivateKey(),
            kTimestamp + 42,
            Bls12381SignatureProvider()
        );

    SignedProtectionBlockProposal tampered(
        otherProposal,
        signedOriginal.proposalSignature()
    );

    const Bls12381SignatureProvider provider;

    requireCondition(
        signedOriginal.isValidForBlockchain(
            blockchain,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Original signed proposal should be valid."
    );

    requireCondition(
        !tampered.isValidForBlockchain(
            blockchain,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Signature must not verify for a different proposal."
    );
}

void testSignatureCannotBeReusedOnDifferentChainTip() {
    Blockchain originalChain =
        baseBlockchain(0);

    Blockchain otherChain =
        baseBlockchain(100);

    const ProtectionBlockProposal proposal =
        proposalFor(originalChain);

    const SignedProtectionBlockProposal signedProposal =
        ValidatorBlockProposalSigner::signProposal(
            proposal,
            "nodo1validatorA",
            validatorPublicKey(),
            validatorPrivateKey(),
            kTimestamp + 50,
            Bls12381SignatureProvider()
        );

    const Bls12381SignatureProvider provider;

    requireCondition(
        signedProposal.isValidForBlockchain(
            originalChain,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Signed proposal should be valid for original chain."
    );

    requireCondition(
        !signedProposal.isValidForBlockchain(
            otherChain,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Signed proposal should not be valid for different chain tip."
    );
}

void testSignaturePayloadCommitsToValidatorAndBlock() {
    Blockchain blockchain =
        baseBlockchain();

    const ProtectionBlockProposal proposal =
        proposalFor(blockchain);

    const std::string payloadA =
        ValidatorBlockProposalSignature::buildSigningPayload(
            proposal,
            "nodo1validatorA",
            validatorPublicKey("a"),
            kTimestamp + 60
        );

    const std::string payloadB =
        ValidatorBlockProposalSignature::buildSigningPayload(
            proposal,
            "nodo1validatorB",
            validatorPublicKey("b"),
            kTimestamp + 60
        );

    requireCondition(
        payloadA != payloadB,
        "Changing validator identity should change signing payload."
    );

    requireCondition(
        payloadA.find(proposal.block().hash()) != std::string::npos,
        "Signing payload should include block hash."
    );

    requireCondition(
        payloadA.find(proposal.expectedPreviousHash()) != std::string::npos,
        "Signing payload should include expected previous hash."
    );
}

void testInvalidSignatureInputsAreRejected() {
    Blockchain blockchain =
        baseBlockchain();

    const ProtectionBlockProposal proposal =
        proposalFor(blockchain);

    bool rejected = false;

    try {
        (void)ValidatorBlockProposalSigner::signProposal(
            proposal,
            "",
            validatorPublicKey(),
            validatorPrivateKey(),
            kTimestamp + 80,
            Bls12381SignatureProvider()
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Empty validator address should be rejected."
    );

    rejected = false;

    try {
        (void)ValidatorBlockProposalSigner::signProposal(
            proposal,
            "nodo1validatorA",
            validatorPublicKey(),
            validatorPrivateKey(),
            0,
            Bls12381SignatureProvider()
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Zero signature timestamp should be rejected."
    );
}

} // namespace

int main() {
    try {
        testValidatorSignsAndVerifiesProposal();
        testSignatureCannotBeReusedOnDifferentProposal();
        testSignatureCannotBeReusedOnDifferentChainTip();
        testSignaturePayloadCommitsToValidatorAndBlock();
        testInvalidSignatureInputsAreRejected();

        std::cout << "Nodo validator block proposal signature tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator block proposal signature tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
