#include "node/SlashingEvidence.hpp"

#include "node/ProtectionRewards.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::LockedStakePosition;
using nodo::node::ProtectionRewardGrant;
using nodo::node::ProtectionRewards;
using nodo::node::ProtectionTreasury;
using nodo::node::ProtectionWorkRecord;
using nodo::node::SecurityScoreCalculator;
using nodo::node::SecurityScoreRecord;
using nodo::node::SlashingEvidence;
using nodo::node::SlashingEvidenceRecord;
using nodo::node::SlashingEvidenceSummary;
using nodo::node::SlashingPreparationRecord;
using nodo::node::ValidatorContainmentDecisionBuilder;
using nodo::node::ValidatorNetworkPolicy;
using nodo::node::ValidatorNetworkPolicyBuilder;
using nodo::node::ValidatorRiskAssessment;
using nodo::node::ValidatorRiskAssessmentBuilder;
using nodo::node::ValidatorSecurityCheckpointBuilder;
using nodo::utils::Amount;

const std::string kValidator = "slashing-validator";

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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

std::vector<ProtectionWorkRecord> workRecords() {
    return ProtectionRewards::buildWorkRecords(
        {grant()},
        {securityScore()},
        {riskAssessment()},
        {networkPolicy()}
    );
}

void testBuildsRiskEvidence() {
    const std::vector<SlashingEvidenceRecord> evidence =
        SlashingEvidence::buildEvidenceRecords(
            {riskAssessment()},
            {networkPolicy()},
            workRecords()
        );

    requireCondition(
        evidence.size() == 1U &&
        evidence.front().isValid() &&
        evidence.front().validatorAddress() == kValidator &&
        evidence.front().severityScore() > 0 &&
        !evidence.front().slashable(),
        "Risk containment should produce non-slashable review evidence."
    );
}

void testBuildsPreparationAndSummary() {
    const std::vector<SlashingEvidenceRecord> evidence =
        SlashingEvidence::buildEvidenceRecords(
            {riskAssessment()},
            {networkPolicy()},
            workRecords()
        );

    const std::vector<SlashingPreparationRecord> preparations =
        SlashingEvidence::buildPreparationRecords(
            evidence,
            {
                LockedStakePosition(
                    kValidator,
                    Amount::fromRawUnits(50),
                    1,
                    101,
                    true,
                    "reward-id"
                )
            }
        );

    const SlashingEvidenceSummary summary =
        SlashingEvidence::buildSummary(
            1,
            evidence,
            preparations
        );

    requireCondition(
        preparations.size() == 1U &&
        preparations.front().isValid() &&
        preparations.front().evidenceCount() == 1U &&
        preparations.front().slashableEvidenceCount() == 0U &&
        preparations.front().preparedPenaltyAmount().rawUnits() == 0,
        "Preparation should preserve review data without applying a penalty."
    );

    requireCondition(
        summary.active() &&
        summary.evidenceCount() == 1U &&
        summary.slashableEvidenceCount() == 0U &&
        summary.preparedPenaltyTotal().rawUnits() == 0,
        "Summary should aggregate slashing evidence without executing slashing."
    );
}

} // namespace

int main() {
    try {
        testBuildsRiskEvidence();
        testBuildsPreparationAndSummary();

        std::cout << "Nodo slashing evidence tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo slashing evidence tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
