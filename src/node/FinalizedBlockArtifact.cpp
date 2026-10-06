#include "node/FinalizedBlockArtifactCodec.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "crypto/Hex.hpp"
#include "crypto/hash.h"
#include "node/FinalizedArtifactSchema.hpp"
#include "node/FinalizedMonetarySectionCodec.hpp"
#include "node/FinalizedTreasurySectionCodec.hpp"
#include "node/FinalizedTreasurySectionValidator.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

FinalizedBlockArtifact::FinalizedBlockArtifact()
    : m_block(std::nullopt), m_postStateRoot(""), m_totalFee(),
      m_rewardDistributions(), m_lockedStakePositions(),
      m_securityScoreRecords(), m_securityCheckpoints(),
      m_validatorRiskAssessments(), m_validatorContainmentDecisions(),
      m_validatorNetworkPolicies(),
      m_monetaryFirewallAudit(MonetaryFirewallAudit::notEvaluated()),
      m_genesisTreasurySnapshot(GenesisTreasurySnapshot::notEvaluated()),
      m_protectionRewardBudget(ProtectionRewardBudget::notEvaluated()),
      m_protectionRewardGrants(), m_protectionWorkRecords(),
      m_protectionRewardSummary(ProtectionRewardSummary::notEvaluated()),
      m_protectionRewardSettlements(),
      m_inflationEpochSnapshot(InflationEpochSnapshot::notEvaluated()),
      m_mintAuthorizationRecord(), m_supplyExpansionRecord(),
      m_feeEconomicBalance(FeeEconomicBalance::notEvaluated()),
      m_feeBurnRecord(FeeBurnRecord::notEvaluated()),
      m_treasuryFeeRecord(TreasuryFeeRecord::notEvaluated()),
      m_slashingEvidenceRecords(), m_slashingPreparationRecords(),
      m_slashingEvidenceSummary(SlashingEvidenceSummary::notEvaluated()),
      m_cryptographicSlashingEvidenceRecords(), m_stakePenaltyRecords(),
      m_cryptographicSlashingSummary(
          CryptographicSlashingSummary::notEvaluated()),
      m_governancePolicySnapshot(GovernancePolicySnapshot::notEvaluated()),
      m_governanceActionGuards(),
      m_governanceSummary(GovernanceSummary::notEvaluated()),
      m_validatorLifecycleRecords(),
      m_epochAccountingRecord(EpochAccountingRecord::notEvaluated()),
      m_validatorLifecycleSummary(ValidatorLifecycleSummary::notEvaluated()),
      m_quorumCertificate(), m_finalizedRecord(), m_treasurySection() {}

FinalizedBlockArtifact::FinalizedBlockArtifact(
    core::Block block, std::string postStateRoot, utils::Amount totalFee,
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
    FeeEconomicBalance feeEconomicBalance, FeeBurnRecord feeBurnRecord,
    TreasuryFeeRecord treasuryFeeRecord,
    std::vector<ValidatorRiskEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord>
        cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary,
    GovernancePolicySnapshot governancePolicySnapshot,
    std::vector<GovernanceActionGuard> governanceActionGuards,
    GovernanceSummary governanceSummary,
    std::vector<ValidatorLifecycleRecord> validatorLifecycleRecords,
    EpochAccountingRecord epochAccountingRecord,
    ValidatorLifecycleSummary validatorLifecycleSummary,
    consensus::QuorumCertificate quorumCertificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    FinalizedTreasurySection treasurySection)
    : m_block(std::move(block)), m_postStateRoot(std::move(postStateRoot)),
      m_totalFee(totalFee),
      m_rewardDistributions(std::move(rewardDistributions)),
      m_lockedStakePositions(std::move(lockedStakePositions)),
      m_securityScoreRecords(std::move(securityScoreRecords)),
      m_securityCheckpoints(std::move(securityCheckpoints)),
      m_validatorRiskAssessments(std::move(validatorRiskAssessments)),
      m_validatorContainmentDecisions(std::move(validatorContainmentDecisions)),
      m_validatorNetworkPolicies(std::move(validatorNetworkPolicies)),
      m_monetaryFirewallAudit(std::move(monetaryFirewallAudit)),
      m_genesisTreasurySnapshot(std::move(genesisTreasurySnapshot)),
      m_protectionRewardBudget(std::move(protectionRewardBudget)),
      m_protectionRewardGrants(std::move(protectionRewardGrants)),
      m_protectionWorkRecords(std::move(protectionWorkRecords)),
      m_protectionRewardSummary(std::move(protectionRewardSummary)),
      m_protectionRewardSettlements(std::move(protectionRewardSettlements)),
      m_inflationEpochSnapshot(std::move(inflationEpochSnapshot)),
      m_mintAuthorizationRecord(std::move(mintAuthorizationRecord)),
      m_supplyExpansionRecord(std::move(supplyExpansionRecord)),
      m_feeEconomicBalance(std::move(feeEconomicBalance)),
      m_feeBurnRecord(std::move(feeBurnRecord)),
      m_treasuryFeeRecord(std::move(treasuryFeeRecord)),
      m_slashingEvidenceRecords(std::move(slashingEvidenceRecords)),
      m_slashingPreparationRecords(std::move(slashingPreparationRecords)),
      m_slashingEvidenceSummary(std::move(slashingEvidenceSummary)),
      m_cryptographicSlashingEvidenceRecords(
          std::move(cryptographicSlashingEvidenceRecords)),
      m_stakePenaltyRecords(std::move(stakePenaltyRecords)),
      m_cryptographicSlashingSummary(std::move(cryptographicSlashingSummary)),
      m_governancePolicySnapshot(std::move(governancePolicySnapshot)),
      m_governanceActionGuards(std::move(governanceActionGuards)),
      m_governanceSummary(std::move(governanceSummary)),
      m_validatorLifecycleRecords(std::move(validatorLifecycleRecords)),
      m_epochAccountingRecord(std::move(epochAccountingRecord)),
      m_validatorLifecycleSummary(std::move(validatorLifecycleSummary)),
      m_quorumCertificate(std::move(quorumCertificate)),
      m_finalizedRecord(std::move(finalizedRecord)),
      m_supplyDelta(std::move(supplyDelta)),
      m_treasurySection(std::move(treasurySection)) {}

const core::Block &FinalizedBlockArtifact::block() const {
  if (!m_block.has_value()) {
    throw std::logic_error("FinalizedBlockArtifact has no block.");
  }

  return m_block.value();
}

const std::string &FinalizedBlockArtifact::postStateRoot() const {
  return m_postStateRoot;
}

utils::Amount FinalizedBlockArtifact::totalFee() const { return m_totalFee; }

const std::vector<RewardDistribution> &
FinalizedBlockArtifact::rewardDistributions() const {
  return m_rewardDistributions;
}

const std::vector<LockedStakePosition> &
FinalizedBlockArtifact::lockedStakePositions() const {
  return m_lockedStakePositions;
}

const std::vector<SecurityScoreRecord> &
FinalizedBlockArtifact::securityScoreRecords() const {
  return m_securityScoreRecords;
}

const std::vector<ValidatorSecurityCheckpoint> &
FinalizedBlockArtifact::securityCheckpoints() const {
  return m_securityCheckpoints;
}

const std::vector<ValidatorRiskAssessment> &
FinalizedBlockArtifact::validatorRiskAssessments() const {
  return m_validatorRiskAssessments;
}

const std::vector<ValidatorContainmentDecision> &
FinalizedBlockArtifact::validatorContainmentDecisions() const {
  return m_validatorContainmentDecisions;
}

const std::vector<ValidatorNetworkPolicy> &
FinalizedBlockArtifact::validatorNetworkPolicies() const {
  return m_validatorNetworkPolicies;
}

const MonetaryFirewallAudit &
FinalizedBlockArtifact::monetaryFirewallAudit() const {
  return m_monetaryFirewallAudit;
}

const GenesisTreasurySnapshot &
FinalizedBlockArtifact::genesisTreasurySnapshot() const {
  return m_genesisTreasurySnapshot;
}

const ProtectionRewardBudget &
FinalizedBlockArtifact::protectionRewardBudget() const {
  return m_protectionRewardBudget;
}

const std::vector<ProtectionRewardGrant> &
FinalizedBlockArtifact::protectionRewardGrants() const {
  return m_protectionRewardGrants;
}

const std::vector<ProtectionWorkRecord> &
FinalizedBlockArtifact::protectionWorkRecords() const {
  return m_protectionWorkRecords;
}

const ProtectionRewardSummary &
FinalizedBlockArtifact::protectionRewardSummary() const {
  return m_protectionRewardSummary;
}

const std::vector<ProtectionRewardSettlement> &
FinalizedBlockArtifact::protectionRewardSettlements() const {
  return m_protectionRewardSettlements;
}

const InflationEpochSnapshot &
FinalizedBlockArtifact::inflationEpochSnapshot() const {
  return m_inflationEpochSnapshot;
}

const MintAuthorizationRecord &
FinalizedBlockArtifact::mintAuthorizationRecord() const {
  return m_mintAuthorizationRecord;
}

const SupplyExpansionRecord &
FinalizedBlockArtifact::supplyExpansionRecord() const {
  return m_supplyExpansionRecord;
}

const FeeEconomicBalance &FinalizedBlockArtifact::feeEconomicBalance() const {
  return m_feeEconomicBalance;
}

const FeeBurnRecord &FinalizedBlockArtifact::feeBurnRecord() const {
  return m_feeBurnRecord;
}

const TreasuryFeeRecord &FinalizedBlockArtifact::treasuryFeeRecord() const {
  return m_treasuryFeeRecord;
}

const std::vector<ValidatorRiskEvidenceRecord> &
FinalizedBlockArtifact::slashingEvidenceRecords() const {
  return m_slashingEvidenceRecords;
}

const std::vector<SlashingPreparationRecord> &
FinalizedBlockArtifact::slashingPreparationRecords() const {
  return m_slashingPreparationRecords;
}

const SlashingEvidenceSummary &
FinalizedBlockArtifact::slashingEvidenceSummary() const {
  return m_slashingEvidenceSummary;
}

const std::vector<CryptographicSlashingEvidenceRecord> &
FinalizedBlockArtifact::cryptographicSlashingEvidenceRecords() const {
  return m_cryptographicSlashingEvidenceRecords;
}

const std::vector<StakePenaltyRecord> &
FinalizedBlockArtifact::stakePenaltyRecords() const {
  return m_stakePenaltyRecords;
}

const CryptographicSlashingSummary &
FinalizedBlockArtifact::cryptographicSlashingSummary() const {
  return m_cryptographicSlashingSummary;
}

const GovernancePolicySnapshot &
FinalizedBlockArtifact::governancePolicySnapshot() const {
  return m_governancePolicySnapshot;
}

const std::vector<GovernanceActionGuard> &
FinalizedBlockArtifact::governanceActionGuards() const {
  return m_governanceActionGuards;
}

const GovernanceSummary &FinalizedBlockArtifact::governanceSummary() const {
  return m_governanceSummary;
}

const std::vector<ValidatorLifecycleRecord> &
FinalizedBlockArtifact::validatorLifecycleRecords() const {
  return m_validatorLifecycleRecords;
}

const EpochAccountingRecord &
FinalizedBlockArtifact::epochAccountingRecord() const {
  return m_epochAccountingRecord;
}

const ValidatorLifecycleSummary &
FinalizedBlockArtifact::validatorLifecycleSummary() const {
  return m_validatorLifecycleSummary;
}

const consensus::QuorumCertificate &
FinalizedBlockArtifact::quorumCertificate() const {
  return m_quorumCertificate;
}

const consensus::FinalizedBlockRecord &
FinalizedBlockArtifact::finalizedRecord() const {
  return m_finalizedRecord;
}

const economics::SupplyDelta &FinalizedBlockArtifact::supplyDelta() const {
  return m_supplyDelta;
}

const FinalizedTreasurySection &
FinalizedBlockArtifact::treasurySection() const {
  return m_treasurySection;
}

/**
 * Performs a comprehensive structural and cryptographic validation of the
 * finalized block artifact. This includes verifying the block itself, fee
 * economics, reward distributions, slashing evidences, governance states, and
 * ensuring that all cross-references (such as checkpoints and stake positions)
 * are consistent with the block's index.
 */
bool FinalizedBlockArtifact::isValid() const {
  if (!m_block.has_value() || !m_block->isValid() || m_postStateRoot.empty() ||
      m_totalFee.isNegative() || !m_quorumCertificate.isStructurallyValid() ||
      !m_finalizedRecord.isStructurallyValid()) {
    return false;
  }

  if (!m_supplyDelta.isValid() ||
      m_supplyDelta.blockHeight() != m_block->index() ||
      m_supplyDelta.blockHash() != m_block->hash()) {
    return false;
  }

  try {
    if (m_totalFee.isZero()) {
      return m_rewardDistributions.empty() && m_lockedStakePositions.empty() &&
             m_securityScoreRecords.empty() && m_securityCheckpoints.empty() &&
             m_validatorRiskAssessments.empty() &&
             m_validatorContainmentDecisions.empty() &&
             m_validatorNetworkPolicies.empty() &&
             m_monetaryFirewallAudit.passed() &&
             m_genesisTreasurySnapshot.active() &&
             m_protectionRewardBudget.active() &&
             m_protectionRewardGrants.empty() &&
             m_protectionWorkRecords.empty() &&
             m_protectionRewardSummary.active() &&
             m_protectionRewardSettlements.empty() &&
             m_inflationEpochSnapshot.active() &&
             m_mintAuthorizationRecord.isValid() &&
             m_supplyExpansionRecord.isValid() &&
             m_supplyExpansionRecord.mintedAmount() ==
                 m_supplyDelta.mintedAmount() &&
             (m_supplyDelta.mintedAmount().isPositive()
                  ? (m_mintAuthorizationRecord.status() == "ACTIVE" &&
                     m_supplyExpansionRecord.status() == "EXECUTED")
                  : (m_mintAuthorizationRecord.status() == "NONE" &&
                     m_supplyExpansionRecord.status() == "NONE")) &&
             FeeEconomics::sameBalance(
                 FeeEconomics::buildFeeEconomicBalance(
                     m_feeEconomicBalance.blockHeight(), m_totalFee),
                 m_feeEconomicBalance) &&
             FeeEconomics::sameBurn(
                 FeeEconomics::buildFeeBurnRecord(
                     m_feeEconomicBalance, m_feeBurnRecord.supplyBefore()),
                 m_feeBurnRecord) &&
             FeeEconomics::sameTreasuryFee(
                 FeeEconomics::buildTreasuryFeeRecord(m_feeEconomicBalance),
                 m_treasuryFeeRecord) &&
             Governance::samePolicy(
                 Governance::buildPolicySnapshot(m_block->index()),
                 m_governancePolicySnapshot) &&
             Governance::sameActionGuards(
                 Governance::buildActionGuards(m_governancePolicySnapshot),
                 m_governanceActionGuards) &&
             m_governanceSummary.active() &&
             m_governanceSummary.blockHeight() == m_block->index() &&
             m_governanceSummary.guardCount() ==
                 m_governanceActionGuards.size() &&
             ValidatorLifecycle::sameLifecycleRecords(
                 ValidatorLifecycle::buildLifecycleRecords(
                     m_block->index(), m_rewardDistributions,
                     m_lockedStakePositions, m_securityScoreRecords,
                     m_protectionRewardSettlements, m_stakePenaltyRecords),
                 m_validatorLifecycleRecords) &&
             ValidatorLifecycle::sameEpochAccounting(
                 ValidatorLifecycle::buildEpochAccountingRecord(
                     m_block->index(), m_validatorLifecycleRecords),
                 m_epochAccountingRecord) &&
             ValidatorLifecycle::sameSummary(
                 ValidatorLifecycle::buildSummary(m_block->index(),
                                                  m_validatorLifecycleRecords,
                                                  m_epochAccountingRecord),
                 m_validatorLifecycleSummary);
    }

    return FeeEconomics::sameBalance(
               FeeEconomics::buildFeeEconomicBalance(
                   m_feeEconomicBalance.blockHeight(), m_totalFee),
               m_feeEconomicBalance) &&
           RewardDistributionCalculator::totalReward(m_rewardDistributions) ==
               m_feeEconomicBalance.validatorRewardAmount() &&
           LockedStakePositionBuilder::samePositions(
               LockedStakePositionBuilder::buildFromRewardDistributions(
                   m_rewardDistributions),
               m_lockedStakePositions) &&
           SecurityScoreCalculator::sameRecords(
               SecurityScoreCalculator::buildFromLockedStakePositions(
                   m_lockedStakePositions, m_block->index()),
               m_securityScoreRecords) &&
           ValidatorSecurityCheckpointBuilder::sameCheckpoints(
               ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
                   m_securityScoreRecords, m_lockedStakePositions,
                   m_block->index()),
               m_securityCheckpoints) &&
           ValidatorRiskAssessmentBuilder::sameAssessments(
               ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
                   m_securityCheckpoints),
               m_validatorRiskAssessments) &&
           ValidatorContainmentDecisionBuilder::sameDecisions(
               ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
                   m_validatorRiskAssessments),
               m_validatorContainmentDecisions) &&
           ValidatorNetworkPolicyBuilder::samePolicies(
               ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
                   m_validatorContainmentDecisions),
               m_validatorNetworkPolicies) &&
           m_monetaryFirewallAudit.passed() &&
           m_genesisTreasurySnapshot.active() &&
           ProtectionTreasury::sameBudget(
               ProtectionTreasury::buildProtectionRewardBudget(
                   m_genesisTreasurySnapshot, m_rewardDistributions),
               m_protectionRewardBudget) &&
           ProtectionTreasury::sameGrants(
               ProtectionTreasury::buildProtectionRewardGrants(
                   m_protectionRewardBudget, m_rewardDistributions,
                   m_securityScoreRecords),
               m_protectionRewardGrants) &&
           ProtectionRewards::sameWorkRecords(
               ProtectionRewards::buildWorkRecords(
                   m_protectionRewardGrants, m_securityScoreRecords,
                   m_validatorRiskAssessments, m_validatorNetworkPolicies),
               m_protectionWorkRecords) &&
           ProtectionRewards::sameSettlements(
               ProtectionRewards::buildSettlements(m_protectionRewardGrants,
                                                   m_protectionWorkRecords),
               m_protectionRewardSettlements) &&
           ProtectionRewards::sameSummary(
               ProtectionRewards::buildSummary(m_protectionRewardBudget,
                                               m_protectionRewardSettlements),
               m_protectionRewardSummary) &&
           m_inflationEpochSnapshot.active() &&
           m_mintAuthorizationRecord.isValid() &&
           m_supplyExpansionRecord.isValid() &&
           m_supplyExpansionRecord.mintedAmount() ==
               m_supplyDelta.mintedAmount() &&
           (m_mintAuthorizationRecord.status() == "ACTIVE"
                ? (ControlledIssuance::sameAuthorization(
                       ControlledIssuance::buildEpochRewardAuthorization(
                           m_inflationEpochSnapshot,
                           m_mintAuthorizationRecord.authorizedAmount(),
                           m_mintAuthorizationRecord.authorizationId(),
                           m_mintAuthorizationRecord.governanceDigest()),
                       m_mintAuthorizationRecord) &&
                   ControlledIssuance::sameExpansion(
                       ControlledIssuance::buildEpochRewardExpansion(
                           m_mintAuthorizationRecord, m_inflationEpochSnapshot),
                       m_supplyExpansionRecord))
                : (ControlledIssuance::sameAuthorization(
                       ControlledIssuance::buildNoMintAuthorization(
                           m_inflationEpochSnapshot),
                       m_mintAuthorizationRecord) &&
                   ControlledIssuance::sameExpansion(
                       ControlledIssuance::buildNoSupplyExpansion(
                           m_mintAuthorizationRecord, m_inflationEpochSnapshot),
                       m_supplyExpansionRecord))) &&
           m_feeEconomicBalance.active() &&
           FeeEconomics::sameBalance(
               FeeEconomics::buildFeeEconomicBalance(
                   m_feeEconomicBalance.blockHeight(), m_totalFee),
               m_feeEconomicBalance) &&
           FeeEconomics::sameBurn(
               FeeEconomics::buildFeeBurnRecord(m_feeEconomicBalance,
                                                m_feeBurnRecord.supplyBefore()),
               m_feeBurnRecord) &&
           FeeEconomics::sameTreasuryFee(
               FeeEconomics::buildTreasuryFeeRecord(m_feeEconomicBalance),
               m_treasuryFeeRecord) &&
           SlashingEvidence::sameEvidenceRecords(
               SlashingEvidence::buildEvidenceRecords(
                   m_validatorRiskAssessments, m_validatorNetworkPolicies,
                   m_protectionWorkRecords),
               m_slashingEvidenceRecords) &&
           SlashingEvidence::samePreparationRecords(
               SlashingEvidence::buildPreparationRecords(
                   m_slashingEvidenceRecords, m_lockedStakePositions),
               m_slashingPreparationRecords) &&
           SlashingEvidence::sameSummary(
               SlashingEvidence::buildSummary(m_block->index(),
                                              m_slashingEvidenceRecords,
                                              m_slashingPreparationRecords),
               m_slashingEvidenceSummary) &&
           CryptographicSlashing::sameEvidenceRecords(
               CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
                   m_quorumCertificate.votes()),
               m_cryptographicSlashingEvidenceRecords) &&
           CryptographicSlashing::sameStakePenaltyRecords(
               CryptographicSlashing::buildStakePenaltyRecords(
                   m_cryptographicSlashingEvidenceRecords,
                   m_lockedStakePositions),
               m_stakePenaltyRecords) &&
           CryptographicSlashing::sameSummary(
               CryptographicSlashing::buildSummary(
                   m_block->index(), m_cryptographicSlashingEvidenceRecords,
                   m_stakePenaltyRecords),
               m_cryptographicSlashingSummary) &&
           Governance::samePolicy(
               Governance::buildPolicySnapshot(m_block->index()),
               m_governancePolicySnapshot) &&
           Governance::sameActionGuards(
               Governance::buildActionGuards(m_governancePolicySnapshot),
               m_governanceActionGuards) &&
           m_governanceSummary.active() &&
           m_governanceSummary.blockHeight() == m_block->index() &&
           m_governanceSummary.guardCount() ==
               m_governanceActionGuards.size() &&
           ValidatorLifecycle::sameLifecycleRecords(
               ValidatorLifecycle::buildLifecycleRecords(
                   m_block->index(), m_rewardDistributions,
                   m_lockedStakePositions, m_securityScoreRecords,
                   m_protectionRewardSettlements, m_stakePenaltyRecords),
               m_validatorLifecycleRecords) &&
           ValidatorLifecycle::sameEpochAccounting(
               ValidatorLifecycle::buildEpochAccountingRecord(
                   m_block->index(), m_validatorLifecycleRecords),
               m_epochAccountingRecord) &&
           ValidatorLifecycle::sameSummary(
               ValidatorLifecycle::buildSummary(m_block->index(),
                                                m_validatorLifecycleRecords,
                                                m_epochAccountingRecord),
               m_validatorLifecycleSummary);
  } catch (const std::exception &) {
    return false;
  }
}

std::string FinalizedBlockArtifact::serialize() const {
  std::ostringstream oss;

  oss << "FinalizedBlockArtifact{"
      << "blockHash="
      << (m_block.has_value() && m_block->isValid() ? m_block->hash()
                                                    : "INVALID")
      << ";postStateRoot=" << m_postStateRoot
      << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
      << ";rewardDistributionCount=" << m_rewardDistributions.size()
      << ";lockedStakePositionCount=" << m_lockedStakePositions.size()
      << ";securityScoreRecordCount=" << m_securityScoreRecords.size()
      << ";securityCheckpointCount=" << m_securityCheckpoints.size()
      << ";validatorRiskAssessmentCount=" << m_validatorRiskAssessments.size()
      << ";validatorContainmentDecisionCount="
      << m_validatorContainmentDecisions.size()
      << ";validatorNetworkPolicyCount=" << m_validatorNetworkPolicies.size()
      << ";monetaryFirewallStatus=" << m_monetaryFirewallAudit.status()
      << ";genesisTreasuryStatus=" << m_genesisTreasurySnapshot.status()
      << ";protectionRewardBudgetStatus=" << m_protectionRewardBudget.status()
      << ";protectionRewardGrantCount=" << m_protectionRewardGrants.size()
      << ";protectionWorkRecordCount=" << m_protectionWorkRecords.size()
      << ";protectionRewardSummaryStatus=" << m_protectionRewardSummary.status()
      << ";protectionRewardSettlementCount="
      << m_protectionRewardSettlements.size()
      << ";inflationEpochStatus=" << m_inflationEpochSnapshot.status()
      << ";mintAuthorizationStatus=" << m_mintAuthorizationRecord.status()
      << ";supplyExpansionStatus=" << m_supplyExpansionRecord.status()
      << ";feeEconomicBalanceStatus=" << m_feeEconomicBalance.status()
      << ";feeBurnStatus=" << m_feeBurnRecord.status()
      << ";treasuryFeeStatus=" << m_treasuryFeeRecord.status()
      << ";slashingEvidenceRecordCount=" << m_slashingEvidenceRecords.size()
      << ";slashingPreparationRecordCount="
      << m_slashingPreparationRecords.size()
      << ";slashingEvidenceSummaryStatus=" << m_slashingEvidenceSummary.status()
      << ";cryptographicSlashingEvidenceCount="
      << m_cryptographicSlashingEvidenceRecords.size()
      << ";stakePenaltyRecordCount=" << m_stakePenaltyRecords.size()
      << ";cryptographicSlashingSummaryStatus="
      << m_cryptographicSlashingSummary.status()
      << ";governancePolicyStatus=" << m_governancePolicySnapshot.status()
      << ";governanceActionGuardCount=" << m_governanceActionGuards.size()
      << ";governanceSummaryStatus=" << m_governanceSummary.status()
      << ";validatorLifecycleRecordCount=" << m_validatorLifecycleRecords.size()
      << ";epochAccountingStatus=" << m_epochAccountingRecord.status()
      << ";validatorLifecycleSummaryStatus="
      << m_validatorLifecycleSummary.status() << "}";

  return oss.str();
}

std::string FinalizedBlockArtifact::artifactDigest() const {
  // Length prefixes bind every section, including finality and evidence, and
  // keep concatenated fields unambiguous.
  std::ostringstream canonical;
  const auto append = [&canonical](const std::string &value) {
    canonical << value.size() << ':' << value;
  };
  const auto appendCollection = [&append](const auto &items) {
    append(std::to_string(items.size()));
    for (const auto &item : items) {
      append(item.serialize());
    }
  };
  append("NODO_FINALIZED_ARTIFACT_DIGEST_V2");
  append(m_block.has_value() ? m_block->serialize() : "INVALID");
  append(m_postStateRoot);
  append(std::to_string(m_totalFee.rawUnits()));
  append(m_supplyDelta.serialize());
  appendCollection(m_rewardDistributions);
  appendCollection(m_lockedStakePositions);
  appendCollection(m_securityScoreRecords);
  appendCollection(m_securityCheckpoints);
  appendCollection(m_validatorRiskAssessments);
  appendCollection(m_validatorContainmentDecisions);
  appendCollection(m_validatorNetworkPolicies);
  append(m_monetaryFirewallAudit.serialize());
  append(m_genesisTreasurySnapshot.serialize());
  append(m_protectionRewardBudget.serialize());
  appendCollection(m_protectionRewardGrants);
  appendCollection(m_protectionWorkRecords);
  append(m_protectionRewardSummary.serialize());
  appendCollection(m_protectionRewardSettlements);
  append(m_inflationEpochSnapshot.serialize());
  append(m_mintAuthorizationRecord.serialize());
  append(m_supplyExpansionRecord.serialize());
  append(m_feeEconomicBalance.serialize());
  append(m_feeBurnRecord.serialize());
  append(m_treasuryFeeRecord.serialize());
  appendCollection(m_slashingEvidenceRecords);
  appendCollection(m_slashingPreparationRecords);
  append(m_slashingEvidenceSummary.serialize());
  appendCollection(m_cryptographicSlashingEvidenceRecords);
  appendCollection(m_stakePenaltyRecords);
  append(m_cryptographicSlashingSummary.serialize());
  append(m_governancePolicySnapshot.serialize());
  appendCollection(m_governanceActionGuards);
  append(m_governanceSummary.serialize());
  appendCollection(m_validatorLifecycleRecords);
  append(m_epochAccountingRecord.serialize());
  append(m_validatorLifecycleSummary.serialize());
  append(m_quorumCertificate.serialize());
  append(m_finalizedRecord.serialize());
  append(m_treasurySection.serialize());

  char buf[NODO_HASH_BUFFER_SIZE] = {};
  const std::string payload = canonical.str();
  nodo_hash_bytes(reinterpret_cast<const unsigned char *>(payload.data()),
                  static_cast<unsigned long long>(payload.size()), buf,
                  NODO_HASH_BUFFER_SIZE);
  return std::string(buf);
}

} // namespace nodo::node
