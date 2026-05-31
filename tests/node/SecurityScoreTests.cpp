#include "node/SecurityScore.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::LockedStakePosition;
using nodo::node::LockedStakePositionBuilder;
using nodo::node::RewardDistribution;
using nodo::node::RewardDistributionCalculator;
using nodo::node::SecurityScoreCalculator;
using nodo::node::SecurityScoreRecord;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LockedStakePosition lockedStake(
    std::int64_t amountRawUnits
) {
    const RewardDistribution reward(
        "validator-a",
        1,
        Amount::fromRawUnits(amountRawUnits * 10),
        Amount::fromRawUnits(amountRawUnits * 9),
        Amount::fromRawUnits(amountRawUnits),
        RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON
    );

    return LockedStakePositionBuilder::buildFromRewardDistribution(
        reward,
        100
    );
}

void testBuildsSecurityScoreFromLockedStake() {
    const SecurityScoreRecord record =
        SecurityScoreCalculator::buildFromLockedStakePosition(
            lockedStake(10),
            1
        );

    requireCondition(
        record.isValid(),
        "Security score record should be valid."
    );

    requireCondition(
        record.validatorAddress() == "validator-a" &&
        record.blockHeight() == 1U &&
        record.score() == 4 &&
        record.lockedStakeScore() == 1 &&
        record.participationScore() == 2 &&
        record.maturityScore() == 0 &&
        record.penaltyScore() == 0,
        "Security score should combine base, locked stake and participation points."
    );
}

void testCapsLockedStakeComponent() {
    const SecurityScoreRecord record =
        SecurityScoreCalculator::buildFromLockedStakePosition(
            lockedStake(100000),
            1
        );

    requireCondition(
        record.lockedStakeScore() == 300 &&
        record.score() == 303,
        "Locked stake score component should be capped."
    );
}

void testMaturityAddsScore() {
    const SecurityScoreRecord record =
        SecurityScoreCalculator::buildFromLockedStakePosition(
            lockedStake(10),
            101
        );

    requireCondition(
        record.maturityScore() == 10 &&
        record.score() == 14,
        "Mature locked stake should add maturity points."
    );
}

void testRejectsInvalidScoreRecord() {
    const SecurityScoreRecord invalid(
        "validator-a",
        1,
        0,
        0,
        0,
        0,
        0,
        SecurityScoreCalculator::LOCKED_STAKE_REWARD_REASON,
        "source"
    );

    requireCondition(
        !invalid.isValid(),
        "Security score record below minimum score should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsSecurityScoreFromLockedStake();
        testCapsLockedStakeComponent();
        testMaturityAddsScore();
        testRejectsInvalidScoreRecord();

        std::cout << "Nodo security score tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo security score tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
