#include "node/ProtectionRewards.hpp"

#include "node/SecurityCheckpoint.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::ProtectionRewardBudget;
using nodo::node::ProtectionRewardGrant;
using nodo::node::ProtectionRewards;
using nodo::node::ProtectionRewardSettlement;
using nodo::node::ProtectionRewardSummary;
using nodo::node::ProtectionTreasury;
using nodo::node::ProtectionWorkRecord;
using nodo::node::SecurityScoreCalculator;
using nodo::node::SecurityScoreRecord;
using nodo::node::ValidatorContainmentDecisionBuilder;
using nodo::node::ValidatorNetworkPolicy;
using nodo::node::ValidatorNetworkPolicyBuilder;
using nodo::node::ValidatorRiskAssessment;
using nodo::node::ValidatorRiskAssessmentBuilder;
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

const std::string kValidator = "protection-validator";

ProtectionRewardBudget budget() {
    return ProtectionRewardBudget(
        "ACTIVE",
        1,
        ProtectionTreasury::TREASURY_ADDRESS,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(500),
        Amount::fromRawUnits(500),
        1,
        ProtectionTreasury::PROTECTION_BUDGET_REASON,
        "treasury-digest"
    );
}

ProtectionRewardGrant grant() {
    return ProtectionRewardGrant(
        kValidator,
        1,
        Amount::fromRawUnits(500),
        303,
        ProtectionTreasury::PROTECTION_GRANT_REASON,
        "budget-digest"
    );
}

SecurityScoreRecord securityScore() {
    return SecurityScoreRecord(
        kValidator,
        1,
        303,
        300,
        2,
        0,
        0,
        SecurityScoreCalculator::LOCKED_STAKE_REWARD_REASON,
        "locked-stake-id"
    );
}

ValidatorRiskAssessment riskAssessment() {
    const std::uint16_t score = 303;
    const std::uint16_t riskScore = 697;
    const std::string riskLevel =
        ValidatorRiskAssessmentBuilder::riskLevelForScore(
            riskScore
        );

    return ValidatorRiskAssessment(
        kValidator,
        1,
        score,
        ValidatorSecurityCheckpointBuilder::bandForScore(score),
        Amount::fromRawUnits(50),
        riskScore,
        riskLevel,
        ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel(riskLevel),
        ValidatorRiskAssessmentBuilder::VALIDATOR_RISK_ASSESSMENT_REASON,
        "checkpoint-digest"
    );
}

ValidatorNetworkPolicy networkPolicy() {
    const std::string containmentMode = "RESTRICT_TRUST";

    return ValidatorNetworkPolicy(
        kValidator,
        1,
        containmentMode,
        ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode(containmentMode),
        ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode(containmentMode),
        ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode(containmentMode),
        ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode(containmentMode),
        ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode(containmentMode),
        ValidatorNetworkPolicyBuilder::requiresManualReviewForContainmentMode(containmentMode),
        ValidatorNetworkPolicyBuilder::VALIDATOR_NETWORK_POLICY_REASON,
        "containment-digest"
    );
}

void testBuildsProtectionWorkRecord() {
    const std::vector<ProtectionWorkRecord> work =
        ProtectionRewards::buildWorkRecords(
            {grant()},
            {securityScore()},
            {riskAssessment()},
            {networkPolicy()}
        );

    requireCondition(
        work.size() == 1U &&
        work.front().isValid() &&
        work.front().validatorAddress() == kValidator &&
        work.front().totalWorkScore() > 0 &&
        work.front().totalWorkScore() <= 1000,
        "Protection work record should be deterministic and bounded."
    );
}

void testBuildsProtectionRewardSettlement() {
    const std::vector<ProtectionWorkRecord> work =
        ProtectionRewards::buildWorkRecords(
            {grant()},
            {securityScore()},
            {riskAssessment()},
            {networkPolicy()}
        );

    const std::vector<ProtectionRewardSettlement> settlements =
        ProtectionRewards::buildSettlements(
            {grant()},
            work
        );

    requireCondition(
        settlements.size() == 1U &&
        settlements.front().isValid() &&
        settlements.front().plannedReward().rawUnits() == 500 &&
        settlements.front().earnedReward().isPositive() &&
        settlements.front().earnedReward() + settlements.front().deferredReward() == settlements.front().plannedReward(),
        "Protection reward settlement should split planned rewards into earned and deferred amounts."
    );
}

void testBuildsProtectionRewardSummary() {
    const std::vector<ProtectionWorkRecord> work =
        ProtectionRewards::buildWorkRecords(
            {grant()},
            {securityScore()},
            {riskAssessment()},
            {networkPolicy()}
        );

    const std::vector<ProtectionRewardSettlement> settlements =
        ProtectionRewards::buildSettlements(
            {grant()},
            work
        );

    const ProtectionRewardSummary summary =
        ProtectionRewards::buildSummary(
            budget(),
            settlements
        );

    requireCondition(
        summary.active() &&
        summary.plannedTotal().rawUnits() == 500 &&
        summary.earnedTotal().isPositive() &&
        summary.earnedTotal() + summary.deferredTotal() == summary.plannedTotal() &&
        summary.beneficiaryCount() == 1,
        "Protection reward summary should account for all settlements."
    );
}

} // namespace

int main() {
    try {
        testBuildsProtectionWorkRecord();
        testBuildsProtectionRewardSettlement();
        testBuildsProtectionRewardSummary();

        std::cout << "Nodo protection rewards tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protection rewards tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
