#include "node/CryptographicSlashing.hpp"

#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "node/LockedStakePosition.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::consensus::ValidatorVoteDecision;
using nodo::consensus::ValidatorVoteRecord;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
using nodo::node::CryptographicSlashing;
using nodo::node::CryptographicSlashingEvidenceRecord;
using nodo::node::CryptographicSlashingSummary;
using nodo::node::LockedStakePosition;
using nodo::node::StakePenaltyRecord;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ValidatorVoteRecord createVote(
    const KeyPair& keyPair,
    const Bls12381SignatureProvider& provider,
    const std::string& blockHash
) {
    return ValidatorVoteRecord::createVote(
        keyPair.address().value(),
        keyPair.publicKey(),
        keyPair.privateKeyForSigningOnly(),
        10,
        blockHash,
        "previous_block_hash",
        1,
        ValidatorVoteDecision::PRECOMMIT,
        "reason_hash",
        1900000000,
        provider
    );
}

void testBuildsDoubleVoteEvidence() {
    const Bls12381SignatureProvider provider;
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair(
            "cryptographic-slashing-validator"
        );

    const std::vector<ValidatorVoteRecord> votes = {
        createVote(keyPair, provider, "block_hash_a"),
        createVote(keyPair, provider, "block_hash_b")
    };

    const std::vector<CryptographicSlashingEvidenceRecord> evidence =
        CryptographicSlashing::buildEvidenceRecords(
            votes,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        evidence.size() == 1U &&
        evidence.front().isValid() &&
        evidence.front().validatorAddress() == keyPair.address().value() &&
        evidence.front().evidenceType() == CryptographicSlashing::DOUBLE_VOTE_EVIDENCE_TYPE &&
        evidence.front().severityScore() == 1000 &&
        evidence.front().penaltyBasisPoints() == 1000,
        "Double signed validator votes should become slashable cryptographic evidence."
    );
}

void testBuildsStakePenaltyFromCryptographicEvidence() {
    const Bls12381SignatureProvider provider;
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair(
            "cryptographic-slashing-penalty-validator"
        );

    const std::vector<ValidatorVoteRecord> votes = {
        createVote(keyPair, provider, "block_hash_a"),
        createVote(keyPair, provider, "block_hash_b")
    };

    const std::vector<CryptographicSlashingEvidenceRecord> evidence =
        CryptographicSlashing::buildEvidenceRecords(
            votes,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    const std::vector<LockedStakePosition> lockedStake = {
        LockedStakePosition(
            keyPair.address().value(),
            Amount::fromRawUnits(1000),
            10,
            100,
            true,
            "test-reward-id"
        )
    };

    const std::vector<StakePenaltyRecord> penalties =
        CryptographicSlashing::buildStakePenaltyRecords(
            evidence,
            lockedStake
        );

    requireCondition(
        penalties.size() == 1U &&
        penalties.front().isValid() &&
        penalties.front().lockedStakeBefore().rawUnits() == 1000 &&
        penalties.front().penaltyAmount().rawUnits() == 100 &&
        penalties.front().lockedStakeAfter().rawUnits() == 900,
        "Cryptographic slashing should prepare a ten percent penalty from slashable locked stake."
    );
}

void testBuildsSummary() {
    const Bls12381SignatureProvider provider;
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair(
            "cryptographic-slashing-summary-validator"
        );

    const std::vector<ValidatorVoteRecord> votes = {
        createVote(keyPair, provider, "block_hash_a"),
        createVote(keyPair, provider, "block_hash_b")
    };

    const std::vector<CryptographicSlashingEvidenceRecord> evidence =
        CryptographicSlashing::buildEvidenceRecords(
            votes,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    const std::vector<LockedStakePosition> lockedStake = {
        LockedStakePosition(
            keyPair.address().value(),
            Amount::fromRawUnits(1000),
            10,
            100,
            true,
            "test-reward-id"
        )
    };

    const std::vector<StakePenaltyRecord> penalties =
        CryptographicSlashing::buildStakePenaltyRecords(
            evidence,
            lockedStake
        );

    const CryptographicSlashingSummary summary =
        CryptographicSlashing::buildSummary(
            10,
            evidence,
            penalties
        );

    requireCondition(
        summary.active() &&
        summary.evidenceCount() == 1U &&
        summary.slashableEvidenceCount() == 1U &&
        summary.maxSeverityScore() == 1000 &&
        summary.penaltyTotal().rawUnits() == 100,
        "Cryptographic slashing summary should account evidence and penalty totals."
    );
}

void testBuildsSerializableSummaryWithoutEvidence() {
    const CryptographicSlashingSummary summary =
        CryptographicSlashing::buildSummary(
            1,
            {},
            {}
        );

    requireCondition(
        summary.active() &&
        summary.evidenceCount() == 0U &&
        summary.slashableEvidenceCount() == 0U &&
        summary.penaltyTotal().rawUnits() == 0 &&
        !summary.sourcePenaltyDigest().empty(),
        "Zero-evidence cryptographic slashing summaries should remain serializable."
    );
}

void testNoPenaltyWithoutLockedStake() {
    const Bls12381SignatureProvider provider;
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair(
            "cryptographic-slashing-no-stake-validator"
        );

    const std::vector<ValidatorVoteRecord> votes = {
        createVote(keyPair, provider, "block_hash_a"),
        createVote(keyPair, provider, "block_hash_b")
    };

    const std::vector<CryptographicSlashingEvidenceRecord> evidence =
        CryptographicSlashing::buildEvidenceRecords(
            votes,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    const std::vector<StakePenaltyRecord> penalties =
        CryptographicSlashing::buildStakePenaltyRecords(
            evidence,
            {}
        );

    requireCondition(
        evidence.size() == 1U && penalties.empty(),
        "Cryptographic evidence without slashable locked stake should not invent a penalty."
    );
}

} // namespace

int main() {
    try {
        testBuildsDoubleVoteEvidence();
        testBuildsStakePenaltyFromCryptographicEvidence();
        testBuildsSummary();
        testBuildsSerializableSummaryWithoutEvidence();
        testNoPenaltyWithoutLockedStake();

        std::cout << "Nodo cryptographic slashing tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo cryptographic slashing tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
