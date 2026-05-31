#include "node/ValidatorContainmentDecision.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/SecurityScore.hpp"
#include "node/ValidatorRiskAssessment.hpp"
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
using nodo::node::ValidatorContainmentDecision;
using nodo::node::ValidatorContainmentDecisionBuilder;
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

std::vector<ValidatorRiskAssessment> riskAssessments() {
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

    const std::vector<ValidatorSecurityCheckpoint> checkpoints =
        ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
            scores,
            lockedStake,
            1
        );

    return ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
        checkpoints
    );
}

void testBuildsContainmentDecisionFromRiskAssessment() {
    const std::vector<ValidatorRiskAssessment> assessments =
        riskAssessments();

    const std::vector<ValidatorContainmentDecision> decisions =
        ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
            assessments
        );

    requireCondition(
        decisions.size() == 1U,
        "One risk assessment should produce one containment decision."
    );

    requireCondition(
        decisions[0].isValid(),
        "Containment decision should be valid."
    );

    requireCondition(
        decisions[0].validatorAddress() == "validator-a" &&
        decisions[0].blockHeight() == 1U &&
        decisions[0].riskLevel() == "HIGH" &&
        decisions[0].recommendedAction() == "QUARANTINE_REVIEW" &&
        decisions[0].containmentMode() == "REVIEW_QUARANTINE" &&
        decisions[0].peerTrustState() == "QUARANTINE_CANDIDATE" &&
        decisions[0].networkAdmissionState() == "REQUIRE_REVIEW" &&
        decisions[0].reason() == ValidatorContainmentDecisionBuilder::VALIDATOR_CONTAINMENT_DECISION_REASON,
        "High-risk assessment should create a quarantine review containment decision."
    );
}

void testDecisionMapping() {
    requireCondition(
        ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction("ALLOW") == "NONE" &&
        ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction("MONITOR") == "OBSERVE" &&
        ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction("LIMIT_TRUST") == "RESTRICT_TRUST" &&
        ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction("QUARANTINE_REVIEW") == "REVIEW_QUARANTINE",
        "Containment modes should map deterministically from recommended actions."
    );

    requireCondition(
        ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode("NONE") == "TRUSTED" &&
        ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode("OBSERVE") == "WATCHED" &&
        ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode("RESTRICT_TRUST") == "LIMITED" &&
        ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode("REVIEW_QUARANTINE") == "QUARANTINE_CANDIDATE",
        "Peer trust states should map deterministically from containment modes."
    );

    requireCondition(
        ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode("NONE") == "ADMIT" &&
        ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode("OBSERVE") == "ADMIT_WITH_AUDIT" &&
        ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode("RESTRICT_TRUST") == "ADMIT_LIMITED" &&
        ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode("REVIEW_QUARANTINE") == "REQUIRE_REVIEW",
        "Network admission states should map deterministically from containment modes."
    );
}

void testRejectsInvalidDecision() {
    const ValidatorContainmentDecision invalid(
        "validator-a",
        1,
        "HIGH",
        "QUARANTINE_REVIEW",
        "NONE",
        "TRUSTED",
        "ADMIT",
        ValidatorContainmentDecisionBuilder::VALIDATOR_CONTAINMENT_DECISION_REASON,
        "source"
    );

    requireCondition(
        !invalid.isValid(),
        "Containment decision with mismatched containment mode should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsContainmentDecisionFromRiskAssessment();
        testDecisionMapping();
        testRejectsInvalidDecision();

        std::cout << "Nodo validator containment decision tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator containment decision tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
