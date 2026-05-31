#include "node/ValidatorLifecycle.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::EpochAccountingRecord;
using nodo::node::LockedStakePosition;
using nodo::node::ProtectionRewardSettlement;
using nodo::node::RewardDistribution;
using nodo::node::SecurityScoreRecord;
using nodo::node::StakePenaltyRecord;
using nodo::node::ValidatorLifecycle;
using nodo::node::ValidatorLifecycleRecord;
using nodo::node::ValidatorLifecycleSummary;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<RewardDistribution> rewards() {
    return {
        RewardDistribution(
            "validator-a",
            1,
            Amount::fromRawUnits(100),
            Amount::fromRawUnits(90),
            Amount::fromRawUnits(10),
            nodo::node::RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON
        )
    };
}

std::vector<LockedStakePosition> lockedStake() {
    return {
        LockedStakePosition(
            "validator-a",
            Amount::fromRawUnits(10),
            1,
            43201,
            true,
            "reward-a"
        )
    };
}

std::vector<SecurityScoreRecord> securityScores() {
    return {
        SecurityScoreRecord(
            "validator-a",
            1,
            750,
            700,
            2,
            0,
            0,
            nodo::node::SecurityScoreCalculator::LOCKED_STAKE_REWARD_REASON,
            "locked-a"
        )
    };
}

std::vector<ProtectionRewardSettlement> protectionSettlements() {
    return {
        ProtectionRewardSettlement(
            "validator-a",
            1,
            Amount::fromRawUnits(30),
            Amount::fromRawUnits(24),
            Amount::fromRawUnits(6),
            800,
            750,
            nodo::node::ProtectionRewards::PROTECTION_SETTLEMENT_REASON,
            "grant-a",
            "work-a"
        )
    };
}

void testBuildsActiveLifecycleRecord() {
    const std::vector<ValidatorLifecycleRecord> records =
        ValidatorLifecycle::buildLifecycleRecords(
            1,
            rewards(),
            lockedStake(),
            securityScores(),
            protectionSettlements(),
            {}
        );

    requireCondition(
        records.size() == 1 &&
        records.front().isValid() &&
        records.front().active() &&
        records.front().epochIndex() == 1 &&
        records.front().lockedStake().rawUnits() == 10 &&
        records.front().earnedReward().rawUnits() == 24 &&
        records.front().slashingPenalty().rawUnits() == 0,
        "Validator lifecycle should create one active record from validator accounting."
    );
}

void testBuildsEpochAndSummary() {
    const std::vector<ValidatorLifecycleRecord> records =
        ValidatorLifecycle::buildLifecycleRecords(
            1,
            rewards(),
            lockedStake(),
            securityScores(),
            protectionSettlements(),
            {}
        );

    const EpochAccountingRecord epoch =
        ValidatorLifecycle::buildEpochAccountingRecord(
            1,
            records
        );

    const ValidatorLifecycleSummary summary =
        ValidatorLifecycle::buildSummary(
            1,
            records,
            epoch
        );

    requireCondition(
        epoch.active() &&
        epoch.epochStartBlock() == 1 &&
        epoch.epochEndBlock() == nodo::node::NODO_VALIDATOR_EPOCH_BLOCKS &&
        epoch.validatorCount() == 1 &&
        epoch.activeValidatorCount() == 1 &&
        summary.active() &&
        summary.activeValidatorCount() == 1 &&
        summary.totalLockedStake().rawUnits() == 10 &&
        summary.totalEarnedRewards().rawUnits() == 24,
        "Epoch accounting and lifecycle summary should aggregate validator lifecycle records."
    );
}

void testPenaltyMovesValidatorToJailed() {
    const std::vector<StakePenaltyRecord> penalties = {
        StakePenaltyRecord(
            "validator-a",
            1,
            Amount::fromRawUnits(10),
            Amount::fromRawUnits(1),
            Amount::fromRawUnits(9),
            1,
            nodo::node::CryptographicSlashing::STAKE_PENALTY_REASON,
            "evidence-a"
        )
    };

    const std::vector<ValidatorLifecycleRecord> records =
        ValidatorLifecycle::buildLifecycleRecords(
            1,
            rewards(),
            lockedStake(),
            securityScores(),
            protectionSettlements(),
            penalties
        );

    requireCondition(
        records.size() == 1 &&
        records.front().jailed() &&
        records.front().slashingPenalty().rawUnits() == 1,
        "A validator with a partial cryptographic stake penalty should be jailed."
    );
}

} // namespace

int main() {
    try {
        testBuildsActiveLifecycleRecord();
        testBuildsEpochAndSummary();
        testPenaltyMovesValidatorToJailed();

        std::cout << "Nodo validator lifecycle tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator lifecycle tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
