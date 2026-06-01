#include "node/MonetaryArtifactValidator.hpp"

#include "node/ControlledIssuance.hpp"
#include "node/FeeEconomics.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/ProtectionTreasury.hpp"

#include <exception>
#include <vector>

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

        const economics::SupplyDelta& supplyDelta =
            artifact.supplyDelta();

        if (!supplyDelta.isValid()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted SupplyDelta is invalid or missing: " +
                supplyDelta.rejectionReason()
            );
        }

        if (supplyDelta.blockHeight() != block.index() ||
            supplyDelta.blockHash() != block.hash()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted SupplyDelta does not match the finalized block identity."
            );
        }

        const economics::MonetaryPolicy supplyPolicy =
            economics::MonetaryPolicy::localnetDefault(
                context.genesisConfig().networkParameters().chainId(),
                MonetaryFirewall::genesisSupply(context.genesisConfig())
            );

        const ArtifactValidationResult supplyGateValidation =
            validateSupplyDelta(
                supplyPolicy,
                supplyDelta,
                {}
            );

        if (!supplyGateValidation.accepted()) {
            return ArtifactValidationResult::rejected(
                prefix + supplyGateValidation.reason()
            );
        }

        const MonetaryFirewallAudit expectedMonetaryAudit =
            MonetaryFirewall::buildAuditWithSupplyBefore(
                block.index(),
                supplyDelta.supplyBefore(),
                supplyDelta.mintedAmount(),
                supplyDelta.burnedAmount(),
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

        const ArtifactValidationResult supplyExpansionConsistency =
            validateSupplyDeltaConsistencyWithSupplyExpansion(
                supplyDelta,
                artifact.supplyExpansionRecord()
            );

        if (!supplyExpansionConsistency.accepted()) {
            return ArtifactValidationResult::rejected(
                prefix + supplyExpansionConsistency.reason()
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
                supplyDelta.supplyBefore()
            );

        if (!FeeEconomics::sameBurn(expectedFeeBurn, artifact.feeBurnRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted fee burn record does not match rebuilt fee split."
            );
        }

        const ArtifactValidationResult feeBurnConsistency =
            validateSupplyDeltaConsistencyWithFeeBurn(
                supplyDelta,
                artifact.feeBurnRecord()
            );

        if (!feeBurnConsistency.accepted()) {
            return ArtifactValidationResult::rejected(
                prefix + feeBurnConsistency.reason()
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

        const ArtifactValidationResult firewallConsistency =
            validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
                supplyDelta,
                artifact.monetaryFirewallAudit()
            );

        if (!firewallConsistency.accepted()) {
            return ArtifactValidationResult::rejected(
                prefix + firewallConsistency.reason()
            );
        }
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

ArtifactValidationResult MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithFeeBurn(
    const economics::SupplyDelta& delta,
    const FeeBurnRecord& feeBurnRecord
) {
    if (!feeBurnRecord.active()) {
        return ArtifactValidationResult::acceptedResult();
    }
    if (delta.burnedAmount() != feeBurnRecord.burnAmount()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: SupplyDelta.burnedAmount (" +
            std::to_string(delta.burnedAmount().rawUnits()) +
            ") does not equal FeeBurnRecord.burnAmount (" +
            std::to_string(feeBurnRecord.burnAmount().rawUnits()) + ")."
        );
    }
    return ArtifactValidationResult::acceptedResult();
}

ArtifactValidationResult MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithSupplyExpansion(
    const economics::SupplyDelta& delta,
    const SupplyExpansionRecord& supplyExpansionRecord
) {
    if (!supplyExpansionRecord.isValid()) {
        return ArtifactValidationResult::acceptedResult();
    }
    if (supplyExpansionRecord.mintedAmount() != delta.mintedAmount()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: SupplyExpansionRecord minted amount (" +
            std::to_string(supplyExpansionRecord.mintedAmount().rawUnits()) +
            ") does not match SupplyDelta.mintedAmount (" +
            std::to_string(delta.mintedAmount().rawUnits()) + ")."
        );
    }

    if (supplyExpansionRecord.mintedAmount().isZero() &&
        !delta.mintRecords().empty()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: SupplyExpansionRecord claims no mint "
            "but SupplyDelta contains mint records."
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

ArtifactValidationResult MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
    const economics::SupplyDelta& delta,
    const MonetaryFirewallAudit& monetaryFirewallAudit
) {
    if (!monetaryFirewallAudit.passed()) {
        if (monetaryFirewallAudit.status() == "NOT_EVALUATED") {
            return ArtifactValidationResult::acceptedResult();
        }

        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: MonetaryFirewallAudit is invalid "
            "or did not pass."
        );
    }

    if (!monetaryFirewallAudit.isValid()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: MonetaryFirewallAudit is invalid."
        );
    }

    const SupplyLedgerSnapshot& ledger =
        monetaryFirewallAudit.supplyLedger();

    if (ledger.blockHeight() != delta.blockHeight() ||
        ledger.supplyBefore() != delta.supplyBefore() ||
        ledger.minted() != delta.mintedAmount() ||
        ledger.burned() != delta.burnedAmount() ||
        ledger.supplyAfter() != delta.supplyAfter()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator: MonetaryFirewallAudit supply ledger "
            "does not match SupplyDelta."
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

ArtifactValidationResult MonetaryArtifactValidator::validateSupplyDelta(
    const economics::MonetaryPolicy& policy,
    const economics::SupplyDelta& delta,
    const std::vector<economics::MintAuthorization>& authorizations
) {
    if (!policy.isValid()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator::validateSupplyDelta: invalid policy: " +
            policy.rejectionReason()
        );
    }
    if (!delta.isValid()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator::validateSupplyDelta: invalid supply delta: " +
            delta.rejectionReason()
        );
    }

    const economics::MonetaryValidationGateResult gateResult =
        economics::MonetaryValidationGate::validate(policy, delta, authorizations);

    if (!gateResult.isAccepted()) {
        return ArtifactValidationResult::rejected(
            "MonetaryArtifactValidator::validateSupplyDelta: gate rejected: " +
            gateResult.reason()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node
