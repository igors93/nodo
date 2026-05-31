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
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

RewardDistribution reward(
    std::int64_t totalRawUnits,
    std::int64_t liquidRawUnits,
    std::int64_t lockedRawUnits
) {
    return RewardDistribution(
        "validator-a",
        10,
        Amount::fromRawUnits(totalRawUnits),
        Amount::fromRawUnits(liquidRawUnits),
        Amount::fromRawUnits(lockedRawUnits),
        RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON
    );
}

void testBuildsLockedStakeFromRewardDistribution() {
    const RewardDistribution distribution =
        reward(
            100,
            90,
            10
        );

    const LockedStakePosition position =
        LockedStakePositionBuilder::buildFromRewardDistribution(
            distribution,
            50
        );

    requireCondition(
        position.isValid(),
        "Locked stake position should be valid."
    );

    requireCondition(
        position.ownerAddress() == "validator-a" &&
        position.amount().rawUnits() == 10 &&
        position.createdAtHeight() == 10U &&
        position.unlockAtHeight() == 60U &&
        position.slashable() &&
        !position.sourceRewardId().empty(),
        "Locked stake position should be derived from locked validator reward."
    );

    requireCondition(
        !position.isMatureAt(59) &&
        position.isMatureAt(60),
        "Locked stake maturity should depend on unlock height."
    );
}

void testBuildsPositionsFromRewardDistributions() {
    const std::vector<RewardDistribution> rewards =
        RewardDistributionCalculator::buildForVoters(
            Amount::fromRawUnits(100),
            {"validator-a"},
            1
        );

    const std::vector<LockedStakePosition> positions =
        LockedStakePositionBuilder::buildFromRewardDistributions(
            rewards,
            100
        );

    requireCondition(
        positions.size() == 1U,
        "One locked reward should create one locked stake position."
    );

    requireCondition(
        LockedStakePositionBuilder::totalLockedAmount(positions).rawUnits() == 10,
        "Locked stake total should equal total locked rewards."
    );
}

void testRejectsInvalidInputs() {
    bool rejectedZeroPeriod = false;

    try {
        (void)LockedStakePositionBuilder::buildFromRewardDistribution(
            reward(
                100,
                90,
                10
            ),
            0
        );
    } catch (const std::invalid_argument&) {
        rejectedZeroPeriod = true;
    }

    requireCondition(
        rejectedZeroPeriod,
        "Locked stake builder should reject zero lock period."
    );

    const LockedStakePosition invalid(
        "",
        Amount::fromRawUnits(10),
        1,
        2,
        true,
        "source"
    );

    requireCondition(
        !invalid.isValid(),
        "Locked stake position with empty owner should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsLockedStakeFromRewardDistribution();
        testBuildsPositionsFromRewardDistributions();
        testRejectsInvalidInputs();

        std::cout << "Nodo locked stake position tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo locked stake position tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
