#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ProtectionBlockProposal.hpp"
#include "core/ValidatorBlockProposalSignature.hpp"
#include "core/ValidatorProposalAdmission.hpp"
#include "core/ValidatorProposalRegistry.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
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
using nodo::core::ValidatorProposalAdmissionPolicy;
using nodo::core::ValidatorProposalAdmissionStatus;
using nodo::core::ValidatorProposalRegistry;
using nodo::core::ValidatorRegistry;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorBlockProposalSigner;
using nodo::crypto::Address;
using nodo::crypto::AddressDerivation;
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

KeyPair validatorKeyPair(
    const std::string& suffix = "default"
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "proposal-admission-validator-key-" + suffix
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
                        "proposal-admission-bootstrap-" + std::to_string(timestampOffset)
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
    std::uint64_t epochId = 1,
    const std::string& acceptedBlockHash = "proposal-admission-accepted-block"
) {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", epochId, 70, "proposal-admission-evidence-a"),
        work("nodo1validatorB", epochId, 30, "proposal-admission-evidence-b")
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

ProtectionBlockProposal proposalFor(
    const Blockchain& blockchain,
    std::int64_t timestamp = kTimestamp + 30,
    const std::string& acceptedBlockHash = "proposal-admission-accepted-block"
) {
    return ProtectionBlockBuilder::buildRewardBlockProposal(
        blockchain,
        rewardDistribution(1, acceptedBlockHash),
        timestamp
    );
}

std::string addressFor(
    const PublicKey& publicKey
) {
    return AddressDerivation::deriveFromPublicKey(publicKey).value();
}

ValidatorRegistrationRecord registrationFor(
    const PublicKey& publicKey,
    std::uint64_t activationEpoch,
    std::int64_t timestamp
) {
    return ValidatorRegistrationRecord(
        addressFor(publicKey),
        publicKey,
        activationEpoch,
        "proposal-admission-metadata-" + publicKey.fingerprint().substr(0, 16),
        timestamp
    );
}

SignedProtectionBlockProposal signedProposalFor(
    const ProtectionBlockProposal& proposal,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp
) {
    return ValidatorBlockProposalSigner::signProposal(
        proposal,
        addressFor(publicKey),
        publicKey,
        privateKey,
        timestamp,
        Bls12381SignatureProvider()
    );
}

void testRegisteredActiveValidatorIsAdmitted() {
    Blockchain blockchain =
        baseBlockchain();

    const PublicKey key =
        validatorPublicKey("active");

    const PrivateKey privateKey =
        validatorPrivateKey("active");

    ValidatorRegistry validatorRegistry;
    requireCondition(
        validatorRegistry.registerValidator(
            registrationFor(key, 1, kTimestamp + 20)
        ).accepted(),
        "Validator registration should be accepted."
    );

    ValidatorProposalRegistry proposalRegistry;

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            proposalFor(blockchain),
            key,
            privateKey,
            kTimestamp + 40
        );

    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    const auto admission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedProposal,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        admission.accepted(),
        "Registered active validator should be admitted."
    );

    requireCondition(
        proposalRegistry.hasProposal(
            addressFor(key),
            signedProposal.proposal().block().index(),
            signedProposal.proposal().block().hash()
        ),
        "Admitted proposal should be registered."
    );
}

void testUnregisteredValidatorIsRejected() {
    Blockchain blockchain =
        baseBlockchain();

    const PublicKey key =
        validatorPublicKey("unregistered");

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            proposalFor(blockchain),
            key,
            validatorPrivateKey("unregistered"),
            kTimestamp + 50
        );

    ValidatorRegistry validatorRegistry;
    ValidatorProposalRegistry proposalRegistry;
    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    const auto admission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedProposal,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        admission.status() == ValidatorProposalAdmissionStatus::UNREGISTERED_VALIDATOR,
        "Unregistered validator must be rejected."
    );

    requireCondition(
        proposalRegistry.entries().empty(),
        "Rejected proposal should not be registered."
    );
}

void testInactiveValidatorIsRejected() {
    Blockchain blockchain =
        baseBlockchain();

    const PublicKey key =
        validatorPublicKey("inactive");

    ValidatorRegistry validatorRegistry;

    requireCondition(
        validatorRegistry.registerValidator(
            registrationFor(key, 1, kTimestamp + 60)
        ).accepted(),
        "Validator registration should be accepted before deactivation."
    );

    requireCondition(
        validatorRegistry.deactivateValidator(
            addressFor(key),
            kTimestamp + 70
        ).deactivated(),
        "Validator deactivation should succeed."
    );

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            proposalFor(blockchain),
            key,
            validatorPrivateKey("inactive"),
            kTimestamp + 80
        );

    ValidatorProposalRegistry proposalRegistry;
    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    const auto admission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedProposal,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        admission.status() == ValidatorProposalAdmissionStatus::INACTIVE_VALIDATOR,
        "Inactive validator must be rejected."
    );
}

void testRegisteredValidatorCannotUseDifferentProposalKey() {
    Blockchain blockchain =
        baseBlockchain();

    const PublicKey registeredKey =
        validatorPublicKey("registered");

    const PublicKey signingKey =
        validatorPublicKey("signing");

    ValidatorRegistry validatorRegistry;

    requireCondition(
        validatorRegistry.registerValidator(
            registrationFor(registeredKey, 1, kTimestamp + 90)
        ).accepted(),
        "Registered key should be accepted."
    );

    /*
     * The proposal is valid for the signing key, but the signing key has no
     * registered validator identity. It must not be admitted.
     */
    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            proposalFor(blockchain),
            signingKey,
            validatorPrivateKey("signing"),
            kTimestamp + 100
        );

    ValidatorProposalRegistry proposalRegistry;
    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    const auto admission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedProposal,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        admission.status() == ValidatorProposalAdmissionStatus::UNREGISTERED_VALIDATOR,
        "Proposal signed by a different unregistered key should be rejected."
    );
}

void testDoubleSignConflictIsDetectedAfterAdmission() {
    Blockchain blockchain =
        baseBlockchain();

    const PublicKey key =
        validatorPublicKey("double-sign");

    const PrivateKey privateKey =
        validatorPrivateKey("double-sign");

    ValidatorRegistry validatorRegistry;

    requireCondition(
        validatorRegistry.registerValidator(
            registrationFor(key, 1, kTimestamp + 110)
        ).accepted(),
        "Validator registration should be accepted."
    );

    const ProtectionBlockProposal proposalA =
        proposalFor(
            blockchain,
            kTimestamp + 120,
            "proposal-admission-double-sign-a"
        );

    const ProtectionBlockProposal proposalB =
        proposalFor(
            blockchain,
            kTimestamp + 121,
            "proposal-admission-double-sign-b"
        );

    requireCondition(
        proposalA.block().index() == proposalB.block().index(),
        "Double-sign fixture should use same block index."
    );

    requireCondition(
        proposalA.block().hash() != proposalB.block().hash(),
        "Double-sign fixture should create different block hashes."
    );

    const SignedProtectionBlockProposal signedA =
        signedProposalFor(
            proposalA,
            key,
            privateKey,
            kTimestamp + 122
        );

    const SignedProtectionBlockProposal signedB =
        signedProposalFor(
            proposalB,
            key,
            privateKey,
            kTimestamp + 123
        );

    ValidatorProposalRegistry proposalRegistry;
    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    const auto firstAdmission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedA,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        firstAdmission.accepted(),
        "First valid proposal should be admitted."
    );

    const auto secondAdmission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedB,
            blockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        secondAdmission.conflictDetected(),
        "Second conflicting proposal should be reported as double-sign."
    );

    requireCondition(
        proposalRegistry.hasDoubleSignConflict(
            addressFor(key),
            signedA.proposal().block().index()
        ),
        "Proposal registry should store double-sign conflict evidence."
    );
}

void testInvalidBlockchainIsRejectedBeforeRegistryMutation() {
    Blockchain emptyBlockchain;

    const PublicKey key =
        validatorPublicKey("invalid-chain");

    ValidatorRegistry validatorRegistry;

    requireCondition(
        validatorRegistry.registerValidator(
            registrationFor(key, 1, kTimestamp + 130)
        ).accepted(),
        "Validator registration should be accepted."
    );

    ValidatorProposalRegistry proposalRegistry;
    const Bls12381SignatureProvider provider;
    const ValidatorProposalAdmissionPolicy admissionPolicy;

    /*
     * Reuse a valid proposal from a different real chain, but validate it
     * against an empty chain. This must fail before registry mutation.
     */
    Blockchain validChain =
        baseBlockchain(50);

    const SignedProtectionBlockProposal signedProposal =
        signedProposalFor(
            proposalFor(validChain),
            key,
            validatorPrivateKey("invalid-chain"),
            kTimestamp + 140
        );

    const auto admission =
        admissionPolicy.admitAndRegisterSignedProposal(
            signedProposal,
            emptyBlockchain,
            validatorRegistry,
            proposalRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        admission.status() == ValidatorProposalAdmissionStatus::INVALID_BLOCKCHAIN,
        "Invalid blockchain should be rejected."
    );

    requireCondition(
        proposalRegistry.entries().empty(),
        "Invalid blockchain admission should not mutate proposal registry."
    );
}

} // namespace

int main() {
    try {
        testRegisteredActiveValidatorIsAdmitted();
        testUnregisteredValidatorIsRejected();
        testInactiveValidatorIsRejected();
        testRegisteredValidatorCannotUseDifferentProposalKey();
        testDoubleSignConflictIsDetectedAfterAdmission();
        testInvalidBlockchainIsRejectedBeforeRegistryMutation();

        std::cout << "Nodo validator proposal admission tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator proposal admission tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
