#include "node/MonetaryArtifactValidator.hpp"

#include "node/ControlledIssuance.hpp"
#include "node/FeeEconomics.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/ProtectionTreasury.hpp"

#include <exception>
#include <vector>

// Task 04 integration point:
// When FinalizedBlockArtifact carries a SupplyDelta, call
// economics::MonetaryValidationGate::validate() here before checking the
// inflation-layer MonetaryFirewallAudit. This ensures the economics-layer
// authorization check (MintAuthorization) is part of artifact validation.
// See economics/MonetaryValidationGate.hpp for the gate interface.

namespace nodo::node {

ArtifactValidationResult MonetaryArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    if (!context.hasExpectedFeeEconomicBalance()) {
        return ArtifactValidationResult::rejected(
            prefix + "Fee economic balance is unavailable for monetary validation."
        );
    }

    try {
        const core::Block& block =
            artifact.block();

        const FeeEconomicBalance& expectedFeeBalance =
            context.expectedFeeEconomicBalance();

        const MonetaryFirewallAudit expectedMonetaryAudit =
            MonetaryFirewall::buildAudit(
                context.genesisConfig(),
                block.index(),
                utils::Amount(),
                artifact.feeBurnRecord().burnAmount(),
                artifact.treasuryFeeRecord().treasuryAmount(),
                utils::Amount()
            );

        if (!MonetaryFirewall::sameAudit(expectedMonetaryAudit, artifact.monetaryFirewallAudit())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted monetary firewall audit does not match rebuilt monetary policy."
            );
        }

        const GenesisTreasurySnapshot expectedTreasurySnapshot =
            ProtectionTreasury::buildGenesisTreasurySnapshot(
                context.genesisConfig(),
                block.index(),
                artifact.treasuryFeeRecord().treasuryAmount()
            );

        if (!ProtectionTreasury::sameTreasurySnapshot(expectedTreasurySnapshot, artifact.genesisTreasurySnapshot())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted genesis treasury snapshot does not match rebuilt genesis treasury."
            );
        }

        const ProtectionRewardBudget expectedProtectionBudget =
            ProtectionTreasury::buildProtectionRewardBudget(
                expectedTreasurySnapshot,
                context.expectedRewards()
            );

        if (!ProtectionTreasury::sameBudget(expectedProtectionBudget, artifact.protectionRewardBudget())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted protection reward budget does not match rebuilt treasury budget."
            );
        }

        const std::vector<ProtectionRewardGrant> expectedProtectionGrants =
            ProtectionTreasury::buildProtectionRewardGrants(
                expectedProtectionBudget,
                context.expectedRewards(),
                context.expectedSecurityScoreRecords()
            );

        if (!ProtectionTreasury::sameGrants(expectedProtectionGrants, artifact.protectionRewardGrants())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted protection reward grants do not match rebuilt reward plan."
            );
        }

        const std::vector<ProtectionWorkRecord> expectedProtectionWorkRecords =
            ProtectionRewards::buildWorkRecords(
                expectedProtectionGrants,
                context.expectedSecurityScoreRecords(),
                context.expectedRiskAssessments(),
                context.expectedNetworkPolicies()
            );

        if (!ProtectionRewards::sameWorkRecords(expectedProtectionWorkRecords, artifact.protectionWorkRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted protection work records do not match rebuilt security context."
            );
        }

        const std::vector<ProtectionRewardSettlement> expectedProtectionSettlements =
            ProtectionRewards::buildSettlements(
                expectedProtectionGrants,
                expectedProtectionWorkRecords
            );

        if (!ProtectionRewards::sameSettlements(expectedProtectionSettlements, artifact.protectionRewardSettlements())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted protection reward settlements do not match rebuilt work records."
            );
        }

        const ProtectionRewardSummary expectedProtectionSummary =
            ProtectionRewards::buildSummary(
                expectedProtectionBudget,
                expectedProtectionSettlements
            );

        if (!ProtectionRewards::sameSummary(expectedProtectionSummary, artifact.protectionRewardSummary())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted protection reward summary does not match rebuilt settlements."
            );
        }

        const InflationEpochSnapshot expectedInflationEpoch =
            ControlledIssuance::buildInflationEpochSnapshot(
                context.genesisConfig(),
                block.index(),
                artifact.monetaryFirewallAudit().annualMintUsedAfter()
            );

        if (!ControlledIssuance::sameEpoch(expectedInflationEpoch, artifact.inflationEpochSnapshot())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted inflation epoch does not match rebuilt controlled issuance policy."
            );
        }

        const MintAuthorizationRecord expectedMintAuthorization =
            ControlledIssuance::buildNoMintAuthorization(expectedInflationEpoch);

        if (!ControlledIssuance::sameAuthorization(expectedMintAuthorization, artifact.mintAuthorizationRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted mint authorization does not match rebuilt controlled issuance policy."
            );
        }

        const SupplyExpansionRecord expectedSupplyExpansion =
            ControlledIssuance::buildNoSupplyExpansion(
                expectedMintAuthorization,
                expectedInflationEpoch
            );

        if (!ControlledIssuance::sameExpansion(expectedSupplyExpansion, artifact.supplyExpansionRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted supply expansion does not match rebuilt controlled issuance policy."
            );
        }

        if (RewardDistributionCalculator::totalReward(artifact.rewardDistributions()) != expectedFeeBalance.validatorRewardAmount()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted rewards do not match validator fee allocation."
            );
        }

        const FeeBurnRecord expectedFeeBurn =
            FeeEconomics::buildFeeBurnRecord(
                expectedFeeBalance,
                artifact.feeBurnRecord().supplyBefore()
            );

        if (!FeeEconomics::sameBurn(expectedFeeBurn, artifact.feeBurnRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted fee burn record does not match rebuilt fee split."
            );
        }

        const TreasuryFeeRecord expectedTreasuryFee =
            FeeEconomics::buildTreasuryFeeRecord(expectedFeeBalance);

        if (!FeeEconomics::sameTreasuryFee(expectedTreasuryFee, artifact.treasuryFeeRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted treasury fee record does not match rebuilt fee split."
            );
        }

        if (artifact.feeBurnRecord().burnAmount() != artifact.monetaryFirewallAudit().supplyLedger().burned() ||
            artifact.feeBurnRecord().supplyBefore() != artifact.monetaryFirewallAudit().supplyLedger().supplyBefore() ||
            artifact.feeBurnRecord().supplyAfter() != artifact.monetaryFirewallAudit().supplyLedger().supplyAfter() ||
            artifact.treasuryFeeRecord().treasuryAmount() != artifact.monetaryFirewallAudit().supplyLedger().treasuryDelta()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted fee economics records do not match monetary firewall audit."
            );
        }
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node

