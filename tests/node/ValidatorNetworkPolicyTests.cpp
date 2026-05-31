#include "node/ValidatorNetworkPolicy.hpp"

#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/SecurityScore.hpp"
#include "node/ValidatorContainmentDecision.hpp"
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
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<ValidatorContainmentDecision> containmentDecisions() {
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

    return ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
        assessments
    );
}

void testBuildsNetworkPolicyFromContainmentDecision() {
    const std::vector<ValidatorContainmentDecision> decisions =
        containmentDecisions();

    const std::vector<ValidatorNetworkPolicy> policies =
        ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
            decisions
        );

    requireCondition(
        policies.size() == 1U,
        "One containment decision should produce one network policy."
    );

    requireCondition(
        policies[0].isValid(),
        "Network policy should be valid."
    );

    requireCondition(
        policies[0].validatorAddress() == "validator-a" &&
        policies[0].blockHeight() == 1U &&
        policies[0].containmentMode() == "REVIEW_QUARANTINE" &&
        policies[0].peerTrustState() == "QUARANTINE_CANDIDATE" &&
        policies[0].networkAdmissionState() == "REQUIRE_REVIEW" &&
        policies[0].connectionPolicy() == "MANUAL_REVIEW_ONLY" &&
        policies[0].messagePolicy() == "BLOCK_UNTIL_REVIEW" &&
        policies[0].consensusPolicy() == "HOLD_FOR_REVIEW" &&
        policies[0].requiresManualReview() &&
        policies[0].reason() == ValidatorNetworkPolicyBuilder::VALIDATOR_NETWORK_POLICY_REASON,
        "Quarantine review containment should derive a manual-review network policy."
    );
}

void testPolicyMapping() {
    requireCondition(
        ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode("NONE") == "FULL_ACCESS" &&
        ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode("OBSERVE") == "AUDITED_ACCESS" &&
        ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode("RESTRICT_TRUST") == "LIMITED_ACCESS" &&
        ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode("REVIEW_QUARANTINE") == "MANUAL_REVIEW_ONLY",
        "Connection policies should map deterministically from containment mode."
    );

    requireCondition(
        ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode("NONE") == "ALLOW_ALL" &&
        ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode("OBSERVE") == "ALLOW_WITH_AUDIT" &&
        ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode("RESTRICT_TRUST") == "RATE_LIMIT_AND_AUDIT" &&
        ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode("REVIEW_QUARANTINE") == "BLOCK_UNTIL_REVIEW",
        "Message policies should map deterministically from containment mode."
    );

    requireCondition(
        ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode("NONE") == "ALLOW_VOTES" &&
        ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode("OBSERVE") == "ALLOW_VOTES_WITH_AUDIT" &&
        ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode("RESTRICT_TRUST") == "REQUIRE_EXTRA_AUDIT" &&
        ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode("REVIEW_QUARANTINE") == "HOLD_FOR_REVIEW",
        "Consensus policies should map deterministically from containment mode."
    );
}

void testRejectsInvalidPolicy() {
    const ValidatorNetworkPolicy invalid(
        "validator-a",
        1,
        "REVIEW_QUARANTINE",
        "QUARANTINE_CANDIDATE",
        "REQUIRE_REVIEW",
        "FULL_ACCESS",
        "ALLOW_ALL",
        "ALLOW_VOTES",
        false,
        ValidatorNetworkPolicyBuilder::VALIDATOR_NETWORK_POLICY_REASON,
        "source"
    );

    requireCondition(
        !invalid.isValid(),
        "Network policy with mismatched fields should be invalid."
    );
}

} // namespace

int main() {
    try {
        testBuildsNetworkPolicyFromContainmentDecision();
        testPolicyMapping();
        testRejectsInvalidPolicy();

        std::cout << "Nodo validator network policy tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator network policy tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
