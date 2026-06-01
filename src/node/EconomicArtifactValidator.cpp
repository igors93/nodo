#include "node/EconomicArtifactValidator.hpp"

#include "node/FeeEconomics.hpp"
#include "node/LockedStakePosition.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/SecurityScore.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "node/ValidatorRiskAssessment.hpp"

#include <exception>
#include <vector>

namespace nodo::node {

ArtifactValidationResult EconomicArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    if (!context.hasStatePreview()) {
        return ArtifactValidationResult::rejected(
            prefix + "State preview result is unavailable for economic validation."
        );
    }

    try {
        const core::Block& block =
            artifact.block();

        const FeeEconomicBalance expectedFeeBalance =
            FeeEconomics::buildFeeEconomicBalance(
                block.index(),
                context.statePreview().totalFee()
            );

        if (!FeeEconomics::sameBalance(expectedFeeBalance, artifact.feeEconomicBalance())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted fee economic balance does not match rebuilt transaction fees."
            );
        }

        context.setExpectedFeeEconomicBalance(expectedFeeBalance);

        const std::vector<RewardDistribution> expectedRewards =
            RewardDistributionCalculator::buildFromQuorumCertificate(
                expectedFeeBalance.validatorRewardAmount(),
                artifact.quorumCertificate(),
                block.index()
            );

        if (!RewardDistributionCalculator::sameDistributions(expectedRewards, artifact.rewardDistributions())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted reward distributions do not match rebuilt validator fee allocation."
            );
        }

        context.setExpectedRewards(expectedRewards);

        const std::vector<LockedStakePosition> expectedLockedStake =
            LockedStakePositionBuilder::buildFromRewardDistributions(expectedRewards);

        if (!LockedStakePositionBuilder::samePositions(expectedLockedStake, artifact.lockedStakePositions())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted locked stake positions do not match rebuilt reward distributions."
            );
        }

        context.setExpectedLockedStakePositions(expectedLockedStake);

        const std::vector<SecurityScoreRecord> expectedScores =
            SecurityScoreCalculator::buildFromLockedStakePositions(
                expectedLockedStake,
                block.index()
            );

        if (!SecurityScoreCalculator::sameRecords(expectedScores, artifact.securityScoreRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted security score records do not match rebuilt locked stake positions."
            );
        }

        context.setExpectedSecurityScoreRecords(expectedScores);

        const std::vector<ValidatorSecurityCheckpoint> expectedCheckpoints =
            ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
                expectedScores,
                expectedLockedStake,
                block.index()
            );

        if (!ValidatorSecurityCheckpointBuilder::sameCheckpoints(expectedCheckpoints, artifact.securityCheckpoints())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted security checkpoints do not match rebuilt security score records."
            );
        }

        context.setExpectedSecurityCheckpoints(expectedCheckpoints);

        const std::vector<ValidatorRiskAssessment> expectedRiskAssessments =
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(expectedCheckpoints);

        if (!ValidatorRiskAssessmentBuilder::sameAssessments(expectedRiskAssessments, artifact.validatorRiskAssessments())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted validator risk assessments do not match rebuilt security checkpoints."
            );
        }

        context.setExpectedRiskAssessments(expectedRiskAssessments);

        const std::vector<ValidatorContainmentDecision> expectedContainmentDecisions =
            ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(expectedRiskAssessments);

        if (!ValidatorContainmentDecisionBuilder::sameDecisions(expectedContainmentDecisions, artifact.validatorContainmentDecisions())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted validator containment decisions do not match rebuilt risk assessments."
            );
        }

        context.setExpectedContainmentDecisions(expectedContainmentDecisions);

        const std::vector<ValidatorNetworkPolicy> expectedNetworkPolicies =
            ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(expectedContainmentDecisions);

        if (!ValidatorNetworkPolicyBuilder::samePolicies(expectedNetworkPolicies, artifact.validatorNetworkPolicies())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted validator network policies do not match rebuilt containment decisions."
            );
        }

        context.setExpectedNetworkPolicies(expectedNetworkPolicies);
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node

