#include "node/ValidatorRiskAssessment.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
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
using nodo::node::ValidatorRiskAssessment;
using nodo::node::ValidatorRiskAssessmentBuilder;
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

std::vector<ValidatorSecurityCheckpoint> checkpoints() {
    const std::vector<RewardDistribution> rewards =
        RewardDistributionCalculator::buildForVoters(
            Amount::fromRawUnits(100),
            {"validator-a"},
            1
        );

    const std::vector<LockedStakePosition> lockedStake =
        LockedStakePositionBuilder::buildFromRewardDistributions(
            rewards,
            100
        );

    const std::vector<SecurityScoreRecord> scores =
        SecurityScoreCalculator::buildFromLockedStakePositions(
            lockedStake,
            1
        );

    return ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
        scores,
        lockedStake,
        1
    );
}

void testBuildsRiskAssessmentFromCheckpoint() {
    const std::vector<ValidatorSecurityCheckpoint> builtCheckpoints =
        checkpoints();

    const std::vector<ValidatorRiskAssessment> assessments =
        ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
            builtCheckpoints
        );

    requireCondition(
        assessments.size() == 1U,
        "One checkpoint should produce one risk assessment."
    );

    requireCondition(
        assessments[0].isValid(),
        "Risk assessment should be valid."
    );

    requireCondition(
        assessments[0].validatorAddress() == "validator-a" &&
        assessments[0].blockHeight() == 1U &&
        assessments[0].score() == 4 &&
        assessments[0].band() == "BUILDING" &&
        assessments[0].lockedStake().rawUnits() == 10 &&
        assessments[0].riskScore() == 996 &&
        assessments[0].riskLevel() == "HIGH" &&
        assessments[0].recommendedAction() == "QUARANTINE_REVIEW" &&
        assessments[0].reason() == ValidatorRiskAssessmentBuilder::VALIDATOR_RISK_ASSESSMENT_REASON,
        "Risk assessment should classify low-score validator as high risk."
    );
}

void testRiskLevelThresholds() {
    requireCondition(
        ValidatorRiskAssessmentBuilder::riskLevelForScore(0) == "LOW" &&
        ValidatorRiskAssessmentBuilder::riskLevelForScore(300) == "MODERATE" &&
        ValidatorRiskAssessmentBuilder::riskLevelForScore(600) == "ELEVATED" &&
        ValidatorRiskAssessmentBuilder::riskLevelForScore(900) == "HIGH",
        "Risk levels should use deterministic thresholds."
    );

    requireCondition(
        ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel("LOW") == "ALLOW" &&
        ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel("MODERATE") == "MONITOR" &&
        ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel("ELEVATED") == "LIMIT_TRUST" &&
        ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel("HIGH") == "QUARANTINE_REVIEW",
        "Recommended actions should match deterministic risk levels."
    );
}

void testRejectsInvalidRiskAssessment() {
    const ValidatorRiskAssessment invalid(
        "validator-a",
        1,
        4,
        "BUILDING",
        Amount::fromRawUnits(10),
        996,
        "LOW",
        "ALLOW",
        ValidatorRiskAssessmentBuilder::VALIDATOR_RISK_ASSESSMENT_REASON,
        "digest"
    );

    requireCondition(
        !invalid.isValid(),
        "Risk assessment with mismatched risk level should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsRiskAssessmentFromCheckpoint();
        testRiskLevelThresholds();
        testRejectsInvalidRiskAssessment();

        std::cout << "Nodo validator risk assessment tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator risk assessment tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
