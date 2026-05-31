#include "node/ValidatorSecurityPosture.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/SecurityScore.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
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
using nodo::node::ValidatorNetworkPolicy;
using nodo::node::ValidatorNetworkPolicyBuilder;
using nodo::node::ValidatorRiskAssessment;
using nodo::node::ValidatorRiskAssessmentBuilder;
using nodo::node::ValidatorSecurityCheckpoint;
using nodo::node::ValidatorSecurityCheckpointBuilder;
using nodo::node::ValidatorSecurityPosture;
using nodo::node::ValidatorSecurityPostureBuilder;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<ValidatorNetworkPolicy> networkPolicies() {
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

    const std::vector<ValidatorRiskAssessment> assessments =
        ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
            checkpoints
        );

    const std::vector<ValidatorContainmentDecision> decisions =
        ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
            assessments
        );

    return ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
        decisions
    );
}

void testBuildsSecurityPostureFromNetworkPolicy() {
    const std::vector<ValidatorNetworkPolicy> policies =
        networkPolicies();

    const std::vector<ValidatorSecurityPosture> postures =
        ValidatorSecurityPostureBuilder::buildFromNetworkPolicies(
            policies
        );

    requireCondition(
        postures.size() == 1U,
        "One network policy should produce one validator security posture."
    );

    requireCondition(
        postures[0].isValid(),
        "Validator security posture should be valid."
    );

    requireCondition(
        postures[0].validatorAddress() == "validator-a" &&
        postures[0].blockHeight() == 1U &&
        postures[0].postureState() == "CONTAINMENT_READY" &&
        postures[0].enforcementReadiness() == "MANUAL_REVIEW_REQUIRED" &&
        postures[0].finalDisposition() == "QUARANTINE_REVIEW_REQUIRED" &&
        postures[0].connectionPolicy() == "MANUAL_REVIEW_ONLY" &&
        postures[0].messagePolicy() == "BLOCK_UNTIL_REVIEW" &&
        postures[0].consensusPolicy() == "HOLD_FOR_REVIEW" &&
        postures[0].requiresManualReview() &&
        postures[0].reason() == ValidatorSecurityPostureBuilder::VALIDATOR_SECURITY_POSTURE_REASON,
        "Manual review network policy should produce containment-ready posture."
    );
}

void testPostureMapping() {
    requireCondition(
        ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy("FULL_ACCESS") == "HEALTHY" &&
        ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy("AUDITED_ACCESS") == "OBSERVED" &&
        ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy("LIMITED_ACCESS") == "CONSTRAINED" &&
        ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy("MANUAL_REVIEW_ONLY") == "CONTAINMENT_READY",
        "Security posture should map deterministically from connection policy."
    );

    requireCondition(
        ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState("HEALTHY") == "ENFORCEMENT_NOT_REQUIRED" &&
        ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState("OBSERVED") == "AUDIT_READY" &&
        ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState("CONSTRAINED") == "LIMITED_ENFORCEMENT_READY" &&
        ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState("CONTAINMENT_READY") == "MANUAL_REVIEW_REQUIRED",
        "Enforcement readiness should map deterministically from posture state."
    );

    requireCondition(
        ValidatorSecurityPostureBuilder::finalDispositionForPostureState("HEALTHY") == "NO_ACTION" &&
        ValidatorSecurityPostureBuilder::finalDispositionForPostureState("OBSERVED") == "AUDIT_ONLY" &&
        ValidatorSecurityPostureBuilder::finalDispositionForPostureState("CONSTRAINED") == "RESTRICTIVE_CONTROL_READY" &&
        ValidatorSecurityPostureBuilder::finalDispositionForPostureState("CONTAINMENT_READY") == "QUARANTINE_REVIEW_REQUIRED",
        "Final disposition should map deterministically from posture state."
    );
}

void testRejectsInvalidPosture() {
    const ValidatorSecurityPosture invalid(
        "validator-a",
        1,
        "HEALTHY",
        "ENFORCEMENT_NOT_REQUIRED",
        "NO_ACTION",
        "MANUAL_REVIEW_ONLY",
        "BLOCK_UNTIL_REVIEW",
        "HOLD_FOR_REVIEW",
        true,
        ValidatorSecurityPostureBuilder::VALIDATOR_SECURITY_POSTURE_REASON,
        "source"
    );

    requireCondition(
        !invalid.isValid(),
        "Security posture with mismatched connection policy should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsSecurityPostureFromNetworkPolicy();
        testPostureMapping();
        testRejectsInvalidPosture();

        std::cout << "Nodo validator security posture tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator security posture tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
