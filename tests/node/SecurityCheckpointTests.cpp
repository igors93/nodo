#include "node/SecurityCheckpoint.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
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
using nodo::node::ValidatorSecurityCheckpoint;
using nodo::node::ValidatorSecurityCheckpointBuilder;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<LockedStakePosition> lockedStakePositions() {
    const std::vector<RewardDistribution> rewards =
        RewardDistributionCalculator::buildForVoters(
            Amount::fromRawUnits(100),
            {"validator-a"},
            1
        );

    return LockedStakePositionBuilder::buildFromRewardDistributions(
        rewards,
        100
    );
}

void testBuildsCheckpointFromSecurityScore() {
    const std::vector<LockedStakePosition> positions =
        lockedStakePositions();

    const std::vector<SecurityScoreRecord> scores =
        SecurityScoreCalculator::buildFromLockedStakePositions(
            positions,
            1
        );

    const std::vector<ValidatorSecurityCheckpoint> checkpoints =
        ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
            scores,
            positions,
            1
        );

    requireCondition(
        checkpoints.size() == 1U,
        "One validator should produce one security checkpoint."
    );

    requireCondition(
        checkpoints[0].isValid(),
        "Security checkpoint should be valid."
    );

    requireCondition(
        checkpoints[0].validatorAddress() == "validator-a" &&
        checkpoints[0].blockHeight() == 1U &&
        checkpoints[0].score() == 4 &&
        checkpoints[0].band() == "BUILDING" &&
        checkpoints[0].lockedStake().rawUnits() == 10 &&
        checkpoints[0].securityScoreRecordCount() == 1U &&
        checkpoints[0].reason() == ValidatorSecurityCheckpointBuilder::SECURITY_CHECKPOINT_REASON,
        "Security checkpoint should consolidate score and locked stake."
    );
}

void testBandClassification() {
    requireCondition(
        ValidatorSecurityCheckpointBuilder::bandForScore(1) == "BUILDING" &&
        ValidatorSecurityCheckpointBuilder::bandForScore(100) == "WATCHED" &&
        ValidatorSecurityCheckpointBuilder::bandForScore(400) == "STABLE" &&
        ValidatorSecurityCheckpointBuilder::bandForScore(700) == "STRONG" &&
        ValidatorSecurityCheckpointBuilder::bandForScore(900) == "ELITE",
        "Security checkpoint bands should use deterministic thresholds."
    );
}

void testRejectsInvalidCheckpoint() {
    const ValidatorSecurityCheckpoint invalid(
        "validator-a",
        1,
        4,
        "WRONG",
        Amount::fromRawUnits(10),
        1,
        ValidatorSecurityCheckpointBuilder::SECURITY_CHECKPOINT_REASON,
        "bad-digest"
    );

    requireCondition(
        !invalid.isValid(),
        "Checkpoint with an invalid band and digest should be rejected."
    );
}

} // namespace

int main() {
    try {
        testBuildsCheckpointFromSecurityScore();
        testBandClassification();
        testRejectsInvalidCheckpoint();

        std::cout << "Nodo security checkpoint tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo security checkpoint tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
