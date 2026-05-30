#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardDistributor.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochEmissionPolicy;
using nodo::economics::EpochRewardDistribution;
using nodo::economics::EpochRewardDistributor;
using nodo::economics::GenesisRewardRecord;
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
    const std::string& evidenceHash,
    ValidationWorkResult result = ValidationWorkResult::ACCEPTED
) {
    return ValidationWorkRecord(
        validator,
        epoch,
        ValidationWorkType::VALIDATE_BLOCK,
        result,
        "target-" + evidenceHash,
        evidenceHash,
        weight,
        kTimestamp
    );
}

ValidatorScoreRecord score(
    const std::string& validator,
    std::uint64_t epoch,
    std::int32_t previousScore,
    std::int32_t newScore
) {
    return ValidatorScoreRecord(
        validator,
        epoch,
        previousScore,
        newScore,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence-" + validator,
        kTimestamp
    );
}

void testDistributorCreatesGenesisRewardsFromAcceptedWork() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 60, "evidence-a"),
        work("nodo1validatorB", 1, 40, "evidence-b"),
        work("nodo1validatorIgnored", 2, 999, "evidence-other-epoch"),
        work("nodo1validatorRejected", 1, 999, "evidence-rejected", ValidationWorkResult::REJECTED)
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", 1, 50, 100),
        score("nodo1validatorB", 1, 50, 50),
        score("nodo1validatorIgnored", 2, 50, 100)
    };

    const EpochRewardDistribution distribution =
        EpochRewardDistributor::distribute(
            1,
            10,
            20,
            Amount::fromNodo(36500),
            Amount::fromNodo(10),
            100,
            policy,
            workRecords,
            scoreRecords,
            "accepted-block-hash",
            kTimestamp
        );

    requireCondition(
        distribution.isValid(),
        "Epoch reward distribution should be valid."
    );

    requireCondition(
        distribution.protectionEpoch().workDemandBasisPoints() == 10000,
        "Work demand should be fully satisfied."
    );

    requireCondition(
        distribution.protectionEpoch().securityEmission() == Amount::fromNodo(4),
        "Security emission should be capped by policy."
    );

    requireCondition(
        distribution.protectionEpoch().rewardPool() == Amount::fromNodo(14),
        "Reward pool should include fees plus security emission."
    );

    requireCondition(
        distribution.distributedSecurityEmission() == Amount::fromNodo(4),
        "Distributor must mint only security emission, not fees."
    );

    requireCondition(
        distribution.validatorRewards().size() == 2U,
        "Two validators should receive rewards."
    );

    requireCondition(
        distribution.validatorRewards()[0].validatorAddress() == "nodo1validatorA",
        "Rewards should be sorted deterministically by validator address."
    );

    requireCondition(
        distribution.validatorRewards()[0].rewardAmount() == Amount::fromNodo(3),
        "Validator A reward should reflect work and score weighting."
    );

    requireCondition(
        distribution.validatorRewards()[1].rewardAmount() == Amount::fromNodo(1),
        "Validator B reward should reflect work and score weighting."
    );

    const std::vector<GenesisRewardRecord> rewards =
        distribution.genesisRewardRecords();

    requireCondition(
        rewards.size() == 2U,
        "Distribution should expose two GenesisReward records."
    );

    requireCondition(
        rewards[0].policyVersion() == policy.policyVersion(),
        "GenesisReward policy version mismatch."
    );

    requireCondition(
        rewards[0].acceptedBlockHash() == "accepted-block-hash",
        "GenesisReward accepted block hash mismatch."
    );

    requireCondition(
        rewards[0].isValid(),
        "GenesisReward record should be valid."
    );
}

void testDistributorUsesBootstrapCapWhenSupplyIsZero() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 50, "bootstrap-evidence-a")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", 1, 50, 100)
    };

    const EpochRewardDistribution distribution =
        EpochRewardDistributor::distribute(
            1,
            0,
            5,
            Amount::fromRawUnits(0),
            Amount::fromRawUnits(0),
            100,
            policy,
            workRecords,
            scoreRecords,
            "bootstrap-block-hash",
            kTimestamp
        );

    requireCondition(
        distribution.protectionEpoch().emissionCap() == Amount::fromNodo(100),
        "Zero supply should use bootstrap emission cap."
    );

    requireCondition(
        distribution.protectionEpoch().workDemandBasisPoints() == 5000,
        "Half target work should use half of the cap."
    );

    requireCondition(
        distribution.distributedSecurityEmission() == Amount::fromNodo(50),
        "Bootstrap distribution should mint half the bootstrap cap."
    );

    requireCondition(
        distribution.validatorRewards().front().rewardAmount() == Amount::fromNodo(50),
        "Single validator should receive full distributed bootstrap reward."
    );
}

void testDistributorRejectsDuplicateAcceptedEvidence() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 10, "same-evidence"),
        work("nodo1validatorA", 1, 10, "same-evidence")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", 1, 50, 100)
    };

    bool rejected = false;

    try {
        (void)EpochRewardDistributor::distribute(
            1,
            0,
            10,
            Amount::fromNodo(36500),
            Amount::fromRawUnits(0),
            100,
            policy,
            workRecords,
            scoreRecords,
            "accepted-block-hash",
            kTimestamp
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Duplicate accepted evidence should be rejected."
    );
}

void testDistributorDoesNotRewardValidatorsWithoutScore() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 100, "evidence-a")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {};

    const EpochRewardDistribution distribution =
        EpochRewardDistributor::distribute(
            1,
            0,
            10,
            Amount::fromNodo(36500),
            Amount::fromRawUnits(0),
            100,
            policy,
            workRecords,
            scoreRecords,
            "accepted-block-hash",
            kTimestamp
        );

    requireCondition(
        distribution.validatorRewards().empty(),
        "Validator without score should not receive minted reward."
    );

    requireCondition(
        distribution.distributedSecurityEmission().isZero(),
        "No-score distribution should mint zero."
    );

    requireCondition(
        distribution.isValid(),
        "No-score distribution should still be structurally valid."
    );
}

void testDistributorRejectsInvalidInputs() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 10, "evidence-a")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", 1, 50, 100)
    };

    bool rejected = false;

    try {
        (void)EpochRewardDistributor::distribute(
            1,
            10,
            9,
            Amount::fromNodo(36500),
            Amount::fromRawUnits(0),
            100,
            policy,
            workRecords,
            scoreRecords,
            "accepted-block-hash",
            kTimestamp
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Epoch with end block lower than start block should be rejected."
    );

    rejected = false;

    try {
        (void)EpochRewardDistributor::distribute(
            1,
            0,
            10,
            Amount::fromNodo(36500),
            Amount::fromRawUnits(0),
            0,
            policy,
            workRecords,
            scoreRecords,
            "accepted-block-hash",
            kTimestamp
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Zero target work should be rejected."
    );
}

} // namespace

int main() {
    try {
        testDistributorCreatesGenesisRewardsFromAcceptedWork();
        testDistributorUsesBootstrapCapWhenSupplyIsZero();
        testDistributorRejectsDuplicateAcceptedEvidence();
        testDistributorDoesNotRewardValidatorsWithoutScore();
        testDistributorRejectsInvalidInputs();

        std::cout << "Nodo epoch reward distributor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo epoch reward distributor tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
