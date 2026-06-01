#ifndef NODO_NODE_FINALIZED_BLOCK_ARTIFACT_HPP
#define NODO_NODE_FINALIZED_BLOCK_ARTIFACT_HPP

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/Block.hpp"
#include "node/ControlledIssuance.hpp"
#include "node/CryptographicSlashing.hpp"
#include "node/FeeEconomics.hpp"
#include "node/Governance.hpp"
#include "node/LockedStakePosition.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/SecurityScore.hpp"
#include "node/SlashingEvidence.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "node/ValidatorRiskAssessment.hpp"
#include "economics/SupplyDelta.hpp"
#include "utils/Amount.hpp"

#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class FinalizedBlockArtifact {
public:
    FinalizedBlockArtifact();

    FinalizedBlockArtifact(
        core::Block block,
        std::string postStateRoot,
        utils::Amount totalFee,
        economics::SupplyDelta supplyDelta,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
        std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
        std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
        MonetaryFirewallAudit monetaryFirewallAudit,
        GenesisTreasurySnapshot genesisTreasurySnapshot,
        ProtectionRewardBudget protectionRewardBudget,
        std::vector<ProtectionRewardGrant> protectionRewardGrants,
        std::vector<ProtectionWorkRecord> protectionWorkRecords,
        ProtectionRewardSummary protectionRewardSummary,
        std::vector<ProtectionRewardSettlement> protectionRewardSettlements,
        InflationEpochSnapshot inflationEpochSnapshot,
        MintAuthorizationRecord mintAuthorizationRecord,
        SupplyExpansionRecord supplyExpansionRecord,
        FeeEconomicBalance feeEconomicBalance,
        FeeBurnRecord feeBurnRecord,
        TreasuryFeeRecord treasuryFeeRecord,
        std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
        std::vector<SlashingPreparationRecord> slashingPreparationRecords,
        SlashingEvidenceSummary slashingEvidenceSummary,
        std::vector<CryptographicSlashingEvidenceRecord> cryptographicSlashingEvidenceRecords,
        std::vector<StakePenaltyRecord> stakePenaltyRecords,
        CryptographicSlashingSummary cryptographicSlashingSummary,
        GovernancePolicySnapshot governancePolicySnapshot,
        std::vector<GovernanceActionGuard> governanceActionGuards,
        GovernanceSummary governanceSummary,
        std::vector<ValidatorLifecycleRecord> validatorLifecycleRecords,
        EpochAccountingRecord epochAccountingRecord,
        ValidatorLifecycleSummary validatorLifecycleSummary,
        consensus::QuorumCertificate quorumCertificate,
        consensus::FinalizedBlockRecord finalizedRecord
    );

    const core::Block& block() const;
    const std::string& postStateRoot() const;
    utils::Amount totalFee() const;
    const std::vector<RewardDistribution>& rewardDistributions() const;
    const std::vector<LockedStakePosition>& lockedStakePositions() const;
    const std::vector<SecurityScoreRecord>& securityScoreRecords() const;
    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints() const;
    const std::vector<ValidatorRiskAssessment>& validatorRiskAssessments() const;
    const std::vector<ValidatorContainmentDecision>& validatorContainmentDecisions() const;
    const std::vector<ValidatorNetworkPolicy>& validatorNetworkPolicies() const;
    const MonetaryFirewallAudit& monetaryFirewallAudit() const;
    const GenesisTreasurySnapshot& genesisTreasurySnapshot() const;
    const ProtectionRewardBudget& protectionRewardBudget() const;
    const std::vector<ProtectionRewardGrant>& protectionRewardGrants() const;
    const std::vector<ProtectionWorkRecord>& protectionWorkRecords() const;
    const ProtectionRewardSummary& protectionRewardSummary() const;
    const std::vector<ProtectionRewardSettlement>& protectionRewardSettlements() const;
    const InflationEpochSnapshot& inflationEpochSnapshot() const;
    const MintAuthorizationRecord& mintAuthorizationRecord() const;
    const SupplyExpansionRecord& supplyExpansionRecord() const;
    const FeeEconomicBalance& feeEconomicBalance() const;
    const FeeBurnRecord& feeBurnRecord() const;
    const TreasuryFeeRecord& treasuryFeeRecord() const;
    const std::vector<SlashingEvidenceRecord>& slashingEvidenceRecords() const;
    const std::vector<SlashingPreparationRecord>& slashingPreparationRecords() const;
    const SlashingEvidenceSummary& slashingEvidenceSummary() const;
    const std::vector<CryptographicSlashingEvidenceRecord>& cryptographicSlashingEvidenceRecords() const;
    const std::vector<StakePenaltyRecord>& stakePenaltyRecords() const;
    const CryptographicSlashingSummary& cryptographicSlashingSummary() const;
    const GovernancePolicySnapshot& governancePolicySnapshot() const;
    const std::vector<GovernanceActionGuard>& governanceActionGuards() const;
    const GovernanceSummary& governanceSummary() const;
    const std::vector<ValidatorLifecycleRecord>& validatorLifecycleRecords() const;
    const EpochAccountingRecord& epochAccountingRecord() const;
    const ValidatorLifecycleSummary& validatorLifecycleSummary() const;
    const consensus::QuorumCertificate& quorumCertificate() const;
    const consensus::FinalizedBlockRecord& finalizedRecord() const;
    const economics::SupplyDelta& supplyDelta() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::optional<core::Block> m_block;
    std::string m_postStateRoot;
    utils::Amount m_totalFee;
    std::vector<RewardDistribution> m_rewardDistributions;
    std::vector<LockedStakePosition> m_lockedStakePositions;
    std::vector<SecurityScoreRecord> m_securityScoreRecords;
    std::vector<ValidatorSecurityCheckpoint> m_securityCheckpoints;
    std::vector<ValidatorRiskAssessment> m_validatorRiskAssessments;
    std::vector<ValidatorContainmentDecision> m_validatorContainmentDecisions;
    std::vector<ValidatorNetworkPolicy> m_validatorNetworkPolicies;
    MonetaryFirewallAudit m_monetaryFirewallAudit;
    GenesisTreasurySnapshot m_genesisTreasurySnapshot;
    ProtectionRewardBudget m_protectionRewardBudget;
    std::vector<ProtectionRewardGrant> m_protectionRewardGrants;
    std::vector<ProtectionWorkRecord> m_protectionWorkRecords;
    ProtectionRewardSummary m_protectionRewardSummary;
    std::vector<ProtectionRewardSettlement> m_protectionRewardSettlements;
    InflationEpochSnapshot m_inflationEpochSnapshot;
    MintAuthorizationRecord m_mintAuthorizationRecord;
    SupplyExpansionRecord m_supplyExpansionRecord;
    FeeEconomicBalance m_feeEconomicBalance;
    FeeBurnRecord m_feeBurnRecord;
    TreasuryFeeRecord m_treasuryFeeRecord;
    std::vector<SlashingEvidenceRecord> m_slashingEvidenceRecords;
    std::vector<SlashingPreparationRecord> m_slashingPreparationRecords;
    SlashingEvidenceSummary m_slashingEvidenceSummary;
    std::vector<CryptographicSlashingEvidenceRecord> m_cryptographicSlashingEvidenceRecords;
    std::vector<StakePenaltyRecord> m_stakePenaltyRecords;
    CryptographicSlashingSummary m_cryptographicSlashingSummary;
    GovernancePolicySnapshot m_governancePolicySnapshot;
    std::vector<GovernanceActionGuard> m_governanceActionGuards;
    GovernanceSummary m_governanceSummary;
    std::vector<ValidatorLifecycleRecord> m_validatorLifecycleRecords;
    EpochAccountingRecord m_epochAccountingRecord;
    ValidatorLifecycleSummary m_validatorLifecycleSummary;
    consensus::QuorumCertificate m_quorumCertificate;
    consensus::FinalizedBlockRecord m_finalizedRecord;
    economics::SupplyDelta m_supplyDelta;
};

} // namespace nodo::node

#endif
