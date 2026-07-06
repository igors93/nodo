#include "node/RuntimeBlockPipeline.hpp"

#include "node/EpochRewardSettlementService.hpp"
#include "node/FeeEconomics.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedSlashingEvidenceAudit.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/RuntimeMonetaryValidation.hpp"
#include "node/StakingRegistry.hpp"
#include "node/StateSnapshot.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/TreasuryExecutionEvidenceBuilder.hpp"
#include "node/ValidatorLifecycle.hpp"

#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteBuilder.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/State.hpp"
#include "core/StateTransitionEngine.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"

#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnitsForRuntime(const NodeRuntime &runtime) {
  const std::uint64_t minimumFee = runtime.effectiveMinimumFeeRawUnits();

  if (minimumFee >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return std::numeric_limits<std::int64_t>::max();
  }

  return static_cast<std::int64_t>(minimumFee);
}

core::StateTransitionPreviewContext
previewContextForRuntime(const NodeRuntime &runtime,
                         std::int64_t wallClockNow = 0) {
  const std::int64_t minimumFee = minimumFeeRawUnitsForRuntime(runtime);

  return RuntimeAccountStateBuilder::previewContextAtTip(runtime, minimumFee,
                                                         wallClockNow);
}

bool rewardDistributionsMatchValidatorReward(
    utils::Amount validatorReward,
    const std::vector<RewardDistribution> &rewardDistributions) {
  try {
    if (validatorReward.isZero()) {
      return rewardDistributions.empty();
    }

    return RewardDistributionCalculator::totalReward(rewardDistributions) ==
           validatorReward;
  } catch (const std::exception &) {
    return false;
  }
}

bool lockedStakePositionsMatchRewards(
    const std::vector<RewardDistribution> &rewardDistributions,
    const std::vector<LockedStakePosition> &lockedStakePositions) {
  try {
    return LockedStakePositionBuilder::samePositions(
        LockedStakePositionBuilder::buildFromRewardDistributions(
            rewardDistributions),
        lockedStakePositions);
  } catch (const std::exception &) {
    return false;
  }
}

bool securityScoreRecordsMatchLockedStake(
    const std::vector<LockedStakePosition> &lockedStakePositions,
    const std::vector<SecurityScoreRecord> &securityScoreRecords,
    std::uint64_t blockHeight) {
  try {
    return SecurityScoreCalculator::sameRecords(
        SecurityScoreCalculator::buildFromLockedStakePositions(
            lockedStakePositions, blockHeight),
        securityScoreRecords);
  } catch (const std::exception &) {
    return false;
  }
}

bool securityCheckpointsMatchScores(
    const std::vector<SecurityScoreRecord> &securityScoreRecords,
    const std::vector<LockedStakePosition> &lockedStakePositions,
    const std::vector<ValidatorSecurityCheckpoint> &securityCheckpoints,
    std::uint64_t blockHeight) {
  try {
    return ValidatorSecurityCheckpointBuilder::sameCheckpoints(
        ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
            securityScoreRecords, lockedStakePositions, blockHeight),
        securityCheckpoints);
  } catch (const std::exception &) {
    return false;
  }
}

bool validatorRiskAssessmentsMatchCheckpoints(
    const std::vector<ValidatorSecurityCheckpoint> &securityCheckpoints,
    const std::vector<ValidatorRiskAssessment> &validatorRiskAssessments) {
  try {
    return ValidatorRiskAssessmentBuilder::sameAssessments(
        ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
            securityCheckpoints),
        validatorRiskAssessments);
  } catch (const std::exception &) {
    return false;
  }
}

bool validatorContainmentDecisionsMatchRisk(
    const std::vector<ValidatorRiskAssessment> &validatorRiskAssessments,
    const std::vector<ValidatorContainmentDecision>
        &validatorContainmentDecisions) {
  try {
    return ValidatorContainmentDecisionBuilder::sameDecisions(
        ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
            validatorRiskAssessments),
        validatorContainmentDecisions);
  } catch (const std::exception &) {
    return false;
  }
}

bool validatorNetworkPoliciesMatchContainment(
    const std::vector<ValidatorContainmentDecision>
        &validatorContainmentDecisions,
    const std::vector<ValidatorNetworkPolicy> &validatorNetworkPolicies) {
  try {
    return ValidatorNetworkPolicyBuilder::samePolicies(
        ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
            validatorContainmentDecisions),
        validatorNetworkPolicies);
  } catch (const std::exception &) {
    return false;
  }
}

bool monetaryFirewallAuditIsStructurallyValid(
    const MonetaryFirewallAudit &audit) {
  return audit.isValid();
}

bool protectionTreasuryPlanIsValid(
    const GenesisTreasurySnapshot &treasurySnapshot,
    const ProtectionRewardBudget &budget,
    const std::vector<ProtectionRewardGrant> &grants,
    const std::vector<RewardDistribution> &rewardDistributions,
    const std::vector<SecurityScoreRecord> &securityScoreRecords) {
  try {
    if (treasurySnapshot.status() == "NOT_EVALUATED" &&
        budget.status() == "NOT_EVALUATED" && grants.empty()) {
      return treasurySnapshot.isValid() && budget.isValid();
    }

    return treasurySnapshot.active() && budget.active() &&
           ProtectionTreasury::sameBudget(
               ProtectionTreasury::buildProtectionRewardBudget(
                   treasurySnapshot, rewardDistributions),
               budget) &&
           ProtectionTreasury::sameGrants(
               ProtectionTreasury::buildProtectionRewardGrants(
                   budget, rewardDistributions, securityScoreRecords),
               grants);
  } catch (const std::exception &) {
    return false;
  }
}
bool protectionRewardsPlanIsValid(
    const ProtectionRewardBudget &budget,
    const std::vector<ProtectionRewardGrant> &grants,
    const std::vector<ProtectionWorkRecord> &workRecords,
    const ProtectionRewardSummary &summary,
    const std::vector<ProtectionRewardSettlement> &settlements,
    const std::vector<SecurityScoreRecord> &securityScoreRecords,
    const std::vector<ValidatorRiskAssessment> &riskAssessments,
    const std::vector<ValidatorNetworkPolicy> &networkPolicies) {
  try {
    return summary.active() &&
           ProtectionRewards::sameWorkRecords(
               ProtectionRewards::buildWorkRecords(grants, securityScoreRecords,
                                                   riskAssessments,
                                                   networkPolicies),
               workRecords) &&
           ProtectionRewards::sameSettlements(
               ProtectionRewards::buildSettlements(grants, workRecords),
               settlements) &&
           ProtectionRewards::sameSummary(
               ProtectionRewards::buildSummary(budget, settlements), summary);
  } catch (const std::exception &) {
    return false;
  }
}

bool slashingEvidencePlanIsValid(
    std::uint64_t blockHeight,
    const std::vector<ValidatorRiskAssessment> &riskAssessments,
    const std::vector<ValidatorNetworkPolicy> &networkPolicies,
    const std::vector<ProtectionWorkRecord> &protectionWorkRecords,
    const std::vector<LockedStakePosition> &lockedStakePositions,
    const std::vector<SlashingEvidenceRecord> &evidenceRecords,
    const std::vector<SlashingPreparationRecord> &preparationRecords,
    const SlashingEvidenceSummary &summary) {
  try {
    const std::vector<SlashingEvidenceRecord> expectedEvidence =
        SlashingEvidence::buildEvidenceRecords(riskAssessments, networkPolicies,
                                               protectionWorkRecords);

    const std::vector<SlashingPreparationRecord> expectedPreparations =
        SlashingEvidence::buildPreparationRecords(expectedEvidence,
                                                  lockedStakePositions);

    const SlashingEvidenceSummary expectedSummary =
        SlashingEvidence::buildSummary(blockHeight, expectedEvidence,
                                       expectedPreparations);

    return SlashingEvidence::sameEvidenceRecords(expectedEvidence,
                                                 evidenceRecords) &&
           SlashingEvidence::samePreparationRecords(expectedPreparations,
                                                    preparationRecords) &&
           SlashingEvidence::sameSummary(expectedSummary, summary);
  } catch (const std::exception &) {
    return false;
  }
}

bool cryptographicSlashingPlanIsValid(
    std::uint64_t blockHeight, const consensus::QuorumCertificate &certificate,
    const std::vector<LockedStakePosition> &lockedStakePositions,
    const std::vector<CryptographicSlashingEvidenceRecord> &evidenceRecords,
    const std::vector<StakePenaltyRecord> &penaltyRecords,
    const CryptographicSlashingSummary &summary) {
  try {
    if (evidenceRecords.empty() && penaltyRecords.empty()) {
      const CryptographicSlashingSummary expectedSummary =
          CryptographicSlashing::buildSummary(blockHeight, {}, {});

      return CryptographicSlashing::sameSummary(expectedSummary, summary);
    }

    const std::vector<CryptographicSlashingEvidenceRecord> expectedEvidence =
        CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
            certificate.votes());

    const std::vector<StakePenaltyRecord> expectedPenalties =
        CryptographicSlashing::buildStakePenaltyRecords(expectedEvidence,
                                                        lockedStakePositions);

    const CryptographicSlashingSummary expectedSummary =
        CryptographicSlashing::buildSummary(blockHeight, expectedEvidence,
                                            expectedPenalties);

    return CryptographicSlashing::sameEvidenceRecords(expectedEvidence,
                                                      evidenceRecords) &&
           CryptographicSlashing::sameStakePenaltyRecords(expectedPenalties,
                                                          penaltyRecords) &&
           CryptographicSlashing::sameSummary(expectedSummary, summary);
  } catch (const std::exception &) {
    return false;
  }
}

bool governancePlanIsValid(std::uint64_t blockHeight,
                           const GovernancePolicySnapshot &policy,
                           const std::vector<GovernanceActionGuard> &guards,
                           const GovernanceSummary &summary) {
  try {
    const GovernancePolicySnapshot expectedPolicy =
        Governance::buildPolicySnapshot(blockHeight);

    const std::vector<GovernanceActionGuard> expectedGuards =
        Governance::buildActionGuards(expectedPolicy);

    return Governance::samePolicy(expectedPolicy, policy) &&
           Governance::sameActionGuards(expectedGuards, guards) &&
           summary.active() && summary.blockHeight() == blockHeight &&
           summary.guardCount() == guards.size() &&
           !summary.sourceGuardDigest().empty();
  } catch (const std::exception &) {
    return false;
  }
}

bool validatorLifecyclePlanIsValid(
    std::uint64_t blockHeight,
    const std::vector<RewardDistribution> &rewardDistributions,
    const std::vector<LockedStakePosition> &lockedStakePositions,
    const std::vector<SecurityScoreRecord> &securityScoreRecords,
    const std::vector<ProtectionRewardSettlement> &protectionRewardSettlements,
    const std::vector<StakePenaltyRecord> &stakePenaltyRecords,
    const std::vector<ValidatorLifecycleRecord> &lifecycleRecords,
    const EpochAccountingRecord &epochAccountingRecord,
    const ValidatorLifecycleSummary &lifecycleSummary) {
  try {
    const std::vector<ValidatorLifecycleRecord> expectedLifecycleRecords =
        ValidatorLifecycle::buildLifecycleRecords(
            blockHeight, rewardDistributions, lockedStakePositions,
            securityScoreRecords, protectionRewardSettlements,
            stakePenaltyRecords);

    const EpochAccountingRecord expectedEpochAccounting =
        ValidatorLifecycle::buildEpochAccountingRecord(
            blockHeight, expectedLifecycleRecords);

    const ValidatorLifecycleSummary expectedSummary =
        ValidatorLifecycle::buildSummary(blockHeight, expectedLifecycleRecords,
                                         expectedEpochAccounting);

    return ValidatorLifecycle::sameLifecycleRecords(expectedLifecycleRecords,
                                                    lifecycleRecords) &&
           ValidatorLifecycle::sameEpochAccounting(expectedEpochAccounting,
                                                   epochAccountingRecord) &&
           ValidatorLifecycle::sameSummary(expectedSummary, lifecycleSummary);
  } catch (const std::exception &) {
    return false;
  }
}

bool controlledIssuancePlanIsValid(const InflationEpochSnapshot &epoch,
                                   const MintAuthorizationRecord &authorization,
                                   const SupplyExpansionRecord &expansion) {
  try {
    if (epoch.status() == "NOT_EVALUATED") {
      return false;
    }

    if (!epoch.active() || !authorization.isValid() || !expansion.isValid()) {
      return false;
    }
    if (authorization.status() == "ACTIVE") {
      return ControlledIssuance::sameAuthorization(
                 ControlledIssuance::buildEpochRewardAuthorization(
                     epoch, authorization.authorizedAmount(),
                     authorization.authorizationId(),
                     authorization.governanceDigest()),
                 authorization) &&
             ControlledIssuance::sameExpansion(
                 ControlledIssuance::buildEpochRewardExpansion(authorization,
                                                               epoch),
                 expansion);
    }
    return ControlledIssuance::sameAuthorization(
               ControlledIssuance::buildNoMintAuthorization(epoch),
               authorization) &&
           ControlledIssuance::sameExpansion(
               ControlledIssuance::buildNoSupplyExpansion(authorization, epoch),
               expansion);
  } catch (const std::exception &) {
    return false;
  }
}
bool feeEconomicsPlanIsValid(utils::Amount totalFee,
                             const FeeEconomicBalance &feeBalance,
                             const FeeBurnRecord &burnRecord,
                             const TreasuryFeeRecord &treasuryRecord) {
  try {
    return feeBalance.active() && burnRecord.active() &&
           treasuryRecord.active() && feeBalance.totalFee() == totalFee &&
           FeeEconomics::sameBalance(FeeEconomics::buildFeeEconomicBalance(
                                         feeBalance.blockHeight(), totalFee),
                                     feeBalance) &&
           FeeEconomics::sameBurn(FeeEconomics::buildFeeBurnRecord(
                                      feeBalance, burnRecord.supplyBefore()),
                                  burnRecord) &&
           FeeEconomics::sameTreasuryFee(
               FeeEconomics::buildTreasuryFeeRecord(feeBalance),
               treasuryRecord);
  } catch (const std::exception &) {
    return false;
  }
}

} // namespace

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig()
    : m_maxTransactionsPerBlock(1000), m_minTransactionsPerBlock(1),
      m_consensusRound(1), m_timestamp(0) {}

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig(
    std::size_t maxTransactionsPerBlock, std::size_t minTransactionsPerBlock,
    std::uint64_t consensusRound, std::int64_t timestamp)
    : m_maxTransactionsPerBlock(maxTransactionsPerBlock),
      m_minTransactionsPerBlock(minTransactionsPerBlock),
      m_consensusRound(consensusRound), m_timestamp(timestamp) {}

std::size_t RuntimeBlockPipelineConfig::maxTransactionsPerBlock() const {
  return m_maxTransactionsPerBlock;
}

std::size_t RuntimeBlockPipelineConfig::minTransactionsPerBlock() const {
  return m_minTransactionsPerBlock;
}

std::uint64_t RuntimeBlockPipelineConfig::consensusRound() const {
  return m_consensusRound;
}

std::int64_t RuntimeBlockPipelineConfig::timestamp() const {
  return m_timestamp;
}

bool RuntimeBlockPipelineConfig::isValid() const {
  return m_maxTransactionsPerBlock > 0 &&
         m_minTransactionsPerBlock <= m_maxTransactionsPerBlock &&
         m_consensusRound > 0 && m_timestamp > 0;
}

std::string RuntimeBlockPipelineConfig::serialize() const {
  std::ostringstream oss;

  oss << "RuntimeBlockPipelineConfig{"
      << "maxTransactionsPerBlock=" << m_maxTransactionsPerBlock
      << ";minTransactionsPerBlock=" << m_minTransactionsPerBlock
      << ";consensusRound=" << m_consensusRound << ";timestamp=" << m_timestamp
      << "}";

  return oss.str();
}

std::string
runtimeBlockPipelineStatusToString(RuntimeBlockPipelineStatus status) {
  switch (status) {
  case RuntimeBlockPipelineStatus::FINALIZED:
    return "FINALIZED";
  case RuntimeBlockPipelineStatus::INVALID_CONFIG:
    return "INVALID_CONFIG";
  case RuntimeBlockPipelineStatus::INVALID_RUNTIME:
    return "INVALID_RUNTIME";
  case RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED:
    return "BLOCK_PRODUCTION_FAILED";
  case RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED:
    return "STATE_TRANSITION_FAILED";
  case RuntimeBlockPipelineStatus::MONETARY_VALIDATION_FAILED:
    return "MONETARY_VALIDATION_FAILED";
  case RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS:
    return "NOT_ENOUGH_VALIDATORS";
  case RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED:
    return "VOTE_BUILD_FAILED";
  case RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED:
    return "QUORUM_BUILD_FAILED";
  case RuntimeBlockPipelineStatus::FINALIZATION_FAILED:
    return "FINALIZATION_FAILED";
  case RuntimeBlockPipelineStatus::PERSISTENCE_FAILED:
    return "PERSISTENCE_FAILED";
  default:
    return "FINALIZATION_FAILED";
  }
}

RuntimeBlockPipelineResult::RuntimeBlockPipelineResult()
    : m_status(RuntimeBlockPipelineStatus::INVALID_CONFIG),
      m_reason("Uninitialized runtime block pipeline result."),
      m_block(std::nullopt), m_certificate(), m_finalizedRecord(),
      m_finalizedTransactionIds(), m_postStateRoot(""), m_totalFee(),
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
      m_validatorLifecycleSummary(ValidatorLifecycleSummary::notEvaluated()) {}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot) {
  return finalized(std::move(block), std::move(certificate),
                   std::move(finalizedRecord),
                   std::move(finalizedTransactionIds), std::move(postStateRoot),
                   utils::Amount(), {}, {}, {}, {}, {}, {}, {});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee) {
  return finalized(std::move(block), std::move(certificate),
                   std::move(finalizedRecord),
                   std::move(finalizedTransactionIds), std::move(postStateRoot),
                   totalFee, {}, {}, {}, {}, {}, {}, {});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions) {
  std::vector<LockedStakePosition> lockedStakePositions;

  try {
    lockedStakePositions =
        LockedStakePositionBuilder::buildFromRewardDistributions(
            rewardDistributions);
  } catch (const std::exception &) {
    lockedStakePositions.clear();
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions) {
  std::vector<SecurityScoreRecord> securityScoreRecords;

  if (!lockedStakePositions.empty() && block.isValid()) {
    try {
      securityScoreRecords =
          SecurityScoreCalculator::buildFromLockedStakePositions(
              lockedStakePositions, block.index());
    } catch (const std::exception &) {
      securityScoreRecords.clear();
    }
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords) {
  std::vector<ValidatorSecurityCheckpoint> securityCheckpoints;

  if (!securityScoreRecords.empty() && block.isValid()) {
    try {
      securityCheckpoints =
          ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
              securityScoreRecords, lockedStakePositions, block.index());
    } catch (const std::exception &) {
      securityCheckpoints.clear();
    }
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints) {
  std::vector<ValidatorRiskAssessment> validatorRiskAssessments;

  try {
    validatorRiskAssessments =
        ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
            securityCheckpoints);
  } catch (const std::exception &) {
    validatorRiskAssessments.clear();
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments) {
  std::vector<ValidatorContainmentDecision> validatorContainmentDecisions;

  try {
    validatorContainmentDecisions =
        ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
            validatorRiskAssessments);
  } catch (const std::exception &) {
    validatorContainmentDecisions.clear();
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions) {
  std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies;

  try {
    validatorNetworkPolicies =
        ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
            validatorContainmentDecisions);
  } catch (const std::exception &) {
    validatorNetworkPolicies.clear();
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies),
      MonetaryFirewallAudit::notEvaluated());
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
    MonetaryFirewallAudit monetaryFirewallAudit) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      GenesisTreasurySnapshot::notEvaluated(),
      ProtectionRewardBudget::notEvaluated(), {});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
    MonetaryFirewallAudit monetaryFirewallAudit,
    GenesisTreasurySnapshot genesisTreasurySnapshot,
    ProtectionRewardBudget protectionRewardBudget,
    std::vector<ProtectionRewardGrant> protectionRewardGrants) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), InflationEpochSnapshot::notEvaluated(),
      MintAuthorizationRecord(), SupplyExpansionRecord());
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    InflationEpochSnapshot inflationEpochSnapshot,
    MintAuthorizationRecord mintAuthorizationRecord,
    SupplyExpansionRecord supplyExpansionRecord) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), {},
      ProtectionRewardSummary::notEvaluated(), {},
      std::move(inflationEpochSnapshot), std::move(mintAuthorizationRecord),
      std::move(supplyExpansionRecord), FeeEconomicBalance::notEvaluated(),
      FeeBurnRecord::notEvaluated(), TreasuryFeeRecord::notEvaluated());
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    TreasuryFeeRecord treasuryFeeRecord) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), std::move(protectionWorkRecords),
      std::move(protectionRewardSummary),
      std::move(protectionRewardSettlements), std::move(inflationEpochSnapshot),
      std::move(mintAuthorizationRecord), std::move(supplyExpansionRecord),
      std::move(feeEconomicBalance), std::move(feeBurnRecord),
      std::move(treasuryFeeRecord), {}, {},
      SlashingEvidenceSummary::notEvaluated());
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), std::move(protectionWorkRecords),
      std::move(protectionRewardSummary),
      std::move(protectionRewardSettlements), std::move(inflationEpochSnapshot),
      std::move(mintAuthorizationRecord), std::move(supplyExpansionRecord),
      std::move(feeEconomicBalance), std::move(feeBurnRecord),
      std::move(treasuryFeeRecord), std::move(slashingEvidenceRecords),
      std::move(slashingPreparationRecords), std::move(slashingEvidenceSummary),
      {}, {}, CryptographicSlashingSummary::notEvaluated());
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord>
        cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary) {
  GovernancePolicySnapshot governancePolicySnapshot;
  std::vector<GovernanceActionGuard> governanceActionGuards;
  GovernanceSummary governanceSummary;
  std::vector<ValidatorLifecycleRecord> validatorLifecycleRecords;
  EpochAccountingRecord epochAccountingRecord;
  ValidatorLifecycleSummary validatorLifecycleSummary;

  try {
    governancePolicySnapshot = Governance::buildPolicySnapshot(block.index());
    governanceActionGuards =
        Governance::buildActionGuards(governancePolicySnapshot);
    governanceSummary =
        Governance::buildSummary(block.index(), governanceActionGuards);
  } catch (const std::exception &) {
    governancePolicySnapshot = GovernancePolicySnapshot::notEvaluated();
    governanceActionGuards.clear();
    governanceSummary = GovernanceSummary::notEvaluated();
  }

  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), std::move(protectionWorkRecords),
      std::move(protectionRewardSummary),
      std::move(protectionRewardSettlements), std::move(inflationEpochSnapshot),
      std::move(mintAuthorizationRecord), std::move(supplyExpansionRecord),
      std::move(feeEconomicBalance), std::move(feeBurnRecord),
      std::move(treasuryFeeRecord), std::move(slashingEvidenceRecords),
      std::move(slashingPreparationRecords), std::move(slashingEvidenceSummary),
      std::move(cryptographicSlashingEvidenceRecords),
      std::move(stakePenaltyRecords), std::move(cryptographicSlashingSummary),
      std::move(governancePolicySnapshot), std::move(governanceActionGuards),
      std::move(governanceSummary));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord>
        cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary,
    GovernancePolicySnapshot governancePolicySnapshot,
    std::vector<GovernanceActionGuard> governanceActionGuards,
    GovernanceSummary governanceSummary) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), std::move(protectionWorkRecords),
      std::move(protectionRewardSummary),
      std::move(protectionRewardSettlements), std::move(inflationEpochSnapshot),
      std::move(mintAuthorizationRecord), std::move(supplyExpansionRecord),
      std::move(feeEconomicBalance), std::move(feeBurnRecord),
      std::move(treasuryFeeRecord), std::move(slashingEvidenceRecords),
      std::move(slashingPreparationRecords), std::move(slashingEvidenceSummary),
      std::move(cryptographicSlashingEvidenceRecords),
      std::move(stakePenaltyRecords), std::move(cryptographicSlashingSummary),
      std::move(governancePolicySnapshot), std::move(governanceActionGuards),
      std::move(governanceSummary), economics::SupplyDelta{});
}

/**
 * Assembles a finalized pipeline result containing all side-effects and
 * evidences generated during the block application process. It also lazily
 * builds required summaries (like cryptographic slashing or governance) if they
 * were not pre-evaluated.
 */
RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord>
        cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary,
    GovernancePolicySnapshot governancePolicySnapshot,
    std::vector<GovernanceActionGuard> governanceActionGuards,
    GovernanceSummary governanceSummary, economics::SupplyDelta supplyDelta) {
  return finalized(
      std::move(block), std::move(certificate), std::move(finalizedRecord),
      std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
      std::move(rewardDistributions), std::move(lockedStakePositions),
      std::move(securityScoreRecords), std::move(securityCheckpoints),
      std::move(validatorRiskAssessments),
      std::move(validatorContainmentDecisions),
      std::move(validatorNetworkPolicies), std::move(monetaryFirewallAudit),
      std::move(genesisTreasurySnapshot), std::move(protectionRewardBudget),
      std::move(protectionRewardGrants), std::move(protectionWorkRecords),
      std::move(protectionRewardSummary),
      std::move(protectionRewardSettlements), std::move(inflationEpochSnapshot),
      std::move(mintAuthorizationRecord), std::move(supplyExpansionRecord),
      std::move(feeEconomicBalance), std::move(feeBurnRecord),
      std::move(treasuryFeeRecord), std::move(slashingEvidenceRecords),
      std::move(slashingPreparationRecords), std::move(slashingEvidenceSummary),
      std::move(cryptographicSlashingEvidenceRecords),
      std::move(stakePenaltyRecords), std::move(cryptographicSlashingSummary),
      std::move(governancePolicySnapshot), std::move(governanceActionGuards),
      std::move(governanceSummary), std::move(supplyDelta),
      std::vector<economics::TreasuryExecutionEvidence>{});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block, consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds, std::string postStateRoot,
    utils::Amount totalFee, std::vector<RewardDistribution> rewardDistributions,
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
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord>
        cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary,
    GovernancePolicySnapshot governancePolicySnapshot,
    std::vector<GovernanceActionGuard> governanceActionGuards,
    GovernanceSummary governanceSummary, economics::SupplyDelta supplyDelta,
    std::vector<economics::TreasuryExecutionEvidence>
        treasuryExecutionEvidence) {
  RuntimeBlockPipelineResult result;
  result.m_status = RuntimeBlockPipelineStatus::FINALIZED;
  result.m_reason = "";
  result.m_block = std::move(block);
  result.m_certificate = std::move(certificate);
  result.m_finalizedRecord = std::move(finalizedRecord);
  result.m_finalizedTransactionIds = std::move(finalizedTransactionIds);
  result.m_postStateRoot = std::move(postStateRoot);
  result.m_totalFee = totalFee;
  result.m_rewardDistributions = std::move(rewardDistributions);
  result.m_lockedStakePositions = std::move(lockedStakePositions);
  result.m_securityScoreRecords = std::move(securityScoreRecords);
  result.m_securityCheckpoints = std::move(securityCheckpoints);
  result.m_validatorRiskAssessments = std::move(validatorRiskAssessments);
  result.m_validatorContainmentDecisions =
      std::move(validatorContainmentDecisions);
  result.m_validatorNetworkPolicies = std::move(validatorNetworkPolicies);
  result.m_monetaryFirewallAudit = std::move(monetaryFirewallAudit);
  result.m_genesisTreasurySnapshot = std::move(genesisTreasurySnapshot);
  result.m_protectionRewardBudget = std::move(protectionRewardBudget);
  result.m_protectionRewardGrants = std::move(protectionRewardGrants);
  result.m_protectionWorkRecords = std::move(protectionWorkRecords);
  result.m_protectionRewardSummary = std::move(protectionRewardSummary);
  result.m_protectionRewardSettlements = std::move(protectionRewardSettlements);
  result.m_inflationEpochSnapshot = std::move(inflationEpochSnapshot);
  result.m_mintAuthorizationRecord = std::move(mintAuthorizationRecord);
  result.m_supplyExpansionRecord = std::move(supplyExpansionRecord);
  result.m_feeEconomicBalance = std::move(feeEconomicBalance);
  result.m_feeBurnRecord = std::move(feeBurnRecord);
  result.m_treasuryFeeRecord = std::move(treasuryFeeRecord);
  result.m_slashingEvidenceRecords = std::move(slashingEvidenceRecords);
  result.m_slashingPreparationRecords = std::move(slashingPreparationRecords);
  result.m_slashingEvidenceSummary = std::move(slashingEvidenceSummary);
  result.m_cryptographicSlashingEvidenceRecords =
      std::move(cryptographicSlashingEvidenceRecords);
  result.m_stakePenaltyRecords = std::move(stakePenaltyRecords);
  result.m_cryptographicSlashingSummary =
      std::move(cryptographicSlashingSummary);
  result.m_governancePolicySnapshot = std::move(governancePolicySnapshot);
  result.m_governanceActionGuards = std::move(governanceActionGuards);
  result.m_governanceSummary = std::move(governanceSummary);
  result.m_supplyDelta = std::move(supplyDelta);
  result.m_treasuryExecutionEvidence = std::move(treasuryExecutionEvidence);

  if (result.m_cryptographicSlashingSummary.status() == "NOT_EVALUATED") {
    result.m_cryptographicSlashingSummary = CryptographicSlashing::buildSummary(
        result.m_block->index(), result.m_cryptographicSlashingEvidenceRecords,
        result.m_stakePenaltyRecords);
  }

  if (result.m_governanceSummary.status() == "NOT_EVALUATED") {
    result.m_governancePolicySnapshot =
        Governance::buildPolicySnapshot(result.m_block->index());
    result.m_governanceActionGuards =
        Governance::buildActionGuards(result.m_governancePolicySnapshot);
    result.m_governanceSummary = Governance::buildSummary(
        result.m_block->index(), result.m_governanceActionGuards);
  }

  result.m_validatorLifecycleRecords =
      ValidatorLifecycle::buildLifecycleRecords(
          result.m_block->index(), result.m_rewardDistributions,
          result.m_lockedStakePositions, result.m_securityScoreRecords,
          result.m_protectionRewardSettlements, result.m_stakePenaltyRecords);

  result.m_epochAccountingRecord =
      ValidatorLifecycle::buildEpochAccountingRecord(
          result.m_block->index(), result.m_validatorLifecycleRecords);

  result.m_validatorLifecycleSummary = ValidatorLifecycle::buildSummary(
      result.m_block->index(), result.m_validatorLifecycleRecords,
      result.m_epochAccountingRecord);

  return result;
}

RuntimeBlockPipelineResult
RuntimeBlockPipelineResult::rejected(RuntimeBlockPipelineStatus status,
                                     std::string reason) {
  RuntimeBlockPipelineResult result;
  result.m_status = status;
  result.m_reason = std::move(reason);
  return result;
}

RuntimeBlockPipelineStatus RuntimeBlockPipelineResult::status() const {
  return m_status;
}

const std::string &RuntimeBlockPipelineResult::reason() const {
  return m_reason;
}

bool RuntimeBlockPipelineResult::finalized() const {
  return m_status == RuntimeBlockPipelineStatus::FINALIZED &&
         m_block.has_value() && m_block->isValid() &&
         m_certificate.isStructurallyValid() &&
         m_finalizedRecord.isStructurallyValid() && m_supplyDelta.isValid() &&
         m_supplyDelta.blockHeight() == m_block->index() &&
         m_supplyDelta.blockHash() == m_block->hash() &&
         !m_postStateRoot.empty() && !m_totalFee.isNegative() &&
         feeEconomicsPlanIsValid(m_totalFee, m_feeEconomicBalance,
                                 m_feeBurnRecord, m_treasuryFeeRecord) &&
         rewardDistributionsMatchValidatorReward(
             m_feeEconomicBalance.validatorRewardAmount(),
             m_rewardDistributions) &&
         lockedStakePositionsMatchRewards(m_rewardDistributions,
                                          m_lockedStakePositions) &&
         securityScoreRecordsMatchLockedStake(m_lockedStakePositions,
                                              m_securityScoreRecords,
                                              m_block->index()) &&
         securityCheckpointsMatchScores(
             m_securityScoreRecords, m_lockedStakePositions,
             m_securityCheckpoints, m_block->index()) &&
         validatorRiskAssessmentsMatchCheckpoints(m_securityCheckpoints,
                                                  m_validatorRiskAssessments) &&
         validatorContainmentDecisionsMatchRisk(
             m_validatorRiskAssessments, m_validatorContainmentDecisions) &&
         validatorNetworkPoliciesMatchContainment(
             m_validatorContainmentDecisions, m_validatorNetworkPolicies) &&
         monetaryFirewallAuditIsStructurallyValid(m_monetaryFirewallAudit) &&
         protectionTreasuryPlanIsValid(
             m_genesisTreasurySnapshot, m_protectionRewardBudget,
             m_protectionRewardGrants, m_rewardDistributions,
             m_securityScoreRecords) &&
         protectionRewardsPlanIsValid(
             m_protectionRewardBudget, m_protectionRewardGrants,
             m_protectionWorkRecords, m_protectionRewardSummary,
             m_protectionRewardSettlements, m_securityScoreRecords,
             m_validatorRiskAssessments, m_validatorNetworkPolicies) &&
         controlledIssuancePlanIsValid(m_inflationEpochSnapshot,
                                       m_mintAuthorizationRecord,
                                       m_supplyExpansionRecord) &&
         m_supplyExpansionRecord.mintedAmount() ==
             m_supplyDelta.mintedAmount() &&
         slashingEvidencePlanIsValid(
             m_block->index(), m_validatorRiskAssessments,
             m_validatorNetworkPolicies, m_protectionWorkRecords,
             m_lockedStakePositions, m_slashingEvidenceRecords,
             m_slashingPreparationRecords, m_slashingEvidenceSummary) &&
         cryptographicSlashingPlanIsValid(
             m_block->index(), m_certificate, m_lockedStakePositions,
             m_cryptographicSlashingEvidenceRecords, m_stakePenaltyRecords,
             m_cryptographicSlashingSummary) &&
         governancePlanIsValid(m_block->index(), m_governancePolicySnapshot,
                               m_governanceActionGuards, m_governanceSummary) &&
         validatorLifecyclePlanIsValid(
             m_block->index(), m_rewardDistributions, m_lockedStakePositions,
             m_securityScoreRecords, m_protectionRewardSettlements,
             m_stakePenaltyRecords, m_validatorLifecycleRecords,
             m_epochAccountingRecord, m_validatorLifecycleSummary);
}

const core::Block &RuntimeBlockPipelineResult::block() const {
  if (!m_block.has_value()) {
    throw std::logic_error(
        "RuntimeBlockPipelineResult has no finalized block.");
  }

  return m_block.value();
}

const consensus::QuorumCertificate &
RuntimeBlockPipelineResult::certificate() const {
  return m_certificate;
}

const consensus::FinalizedBlockRecord &
RuntimeBlockPipelineResult::finalizedRecord() const {
  return m_finalizedRecord;
}

const std::vector<std::string> &
RuntimeBlockPipelineResult::finalizedTransactionIds() const {
  return m_finalizedTransactionIds;
}

const std::string &RuntimeBlockPipelineResult::postStateRoot() const {
  return m_postStateRoot;
}

utils::Amount RuntimeBlockPipelineResult::totalFee() const {
  return m_totalFee;
}

const std::vector<RewardDistribution> &
RuntimeBlockPipelineResult::rewardDistributions() const {
  return m_rewardDistributions;
}

const std::vector<LockedStakePosition> &
RuntimeBlockPipelineResult::lockedStakePositions() const {
  return m_lockedStakePositions;
}

const std::vector<SecurityScoreRecord> &
RuntimeBlockPipelineResult::securityScoreRecords() const {
  return m_securityScoreRecords;
}

const std::vector<ValidatorSecurityCheckpoint> &
RuntimeBlockPipelineResult::securityCheckpoints() const {
  return m_securityCheckpoints;
}

const std::vector<ValidatorRiskAssessment> &
RuntimeBlockPipelineResult::validatorRiskAssessments() const {
  return m_validatorRiskAssessments;
}

const std::vector<ValidatorContainmentDecision> &
RuntimeBlockPipelineResult::validatorContainmentDecisions() const {
  return m_validatorContainmentDecisions;
}

const std::vector<ValidatorNetworkPolicy> &
RuntimeBlockPipelineResult::validatorNetworkPolicies() const {
  return m_validatorNetworkPolicies;
}

const MonetaryFirewallAudit &
RuntimeBlockPipelineResult::monetaryFirewallAudit() const {
  return m_monetaryFirewallAudit;
}

const GenesisTreasurySnapshot &
RuntimeBlockPipelineResult::genesisTreasurySnapshot() const {
  return m_genesisTreasurySnapshot;
}

const ProtectionRewardBudget &
RuntimeBlockPipelineResult::protectionRewardBudget() const {
  return m_protectionRewardBudget;
}

const std::vector<ProtectionRewardGrant> &
RuntimeBlockPipelineResult::protectionRewardGrants() const {
  return m_protectionRewardGrants;
}

const std::vector<ProtectionWorkRecord> &
RuntimeBlockPipelineResult::protectionWorkRecords() const {
  return m_protectionWorkRecords;
}

const ProtectionRewardSummary &
RuntimeBlockPipelineResult::protectionRewardSummary() const {
  return m_protectionRewardSummary;
}

const std::vector<ProtectionRewardSettlement> &
RuntimeBlockPipelineResult::protectionRewardSettlements() const {
  return m_protectionRewardSettlements;
}

const InflationEpochSnapshot &
RuntimeBlockPipelineResult::inflationEpochSnapshot() const {
  return m_inflationEpochSnapshot;
}

const MintAuthorizationRecord &
RuntimeBlockPipelineResult::mintAuthorizationRecord() const {
  return m_mintAuthorizationRecord;
}

const SupplyExpansionRecord &
RuntimeBlockPipelineResult::supplyExpansionRecord() const {
  return m_supplyExpansionRecord;
}

const FeeEconomicBalance &
RuntimeBlockPipelineResult::feeEconomicBalance() const {
  return m_feeEconomicBalance;
}

const FeeBurnRecord &RuntimeBlockPipelineResult::feeBurnRecord() const {
  return m_feeBurnRecord;
}

const TreasuryFeeRecord &RuntimeBlockPipelineResult::treasuryFeeRecord() const {
  return m_treasuryFeeRecord;
}

const std::vector<SlashingEvidenceRecord> &
RuntimeBlockPipelineResult::slashingEvidenceRecords() const {
  return m_slashingEvidenceRecords;
}

const std::vector<SlashingPreparationRecord> &
RuntimeBlockPipelineResult::slashingPreparationRecords() const {
  return m_slashingPreparationRecords;
}

const SlashingEvidenceSummary &
RuntimeBlockPipelineResult::slashingEvidenceSummary() const {
  return m_slashingEvidenceSummary;
}

const std::vector<CryptographicSlashingEvidenceRecord> &
RuntimeBlockPipelineResult::cryptographicSlashingEvidenceRecords() const {
  return m_cryptographicSlashingEvidenceRecords;
}

const std::vector<StakePenaltyRecord> &
RuntimeBlockPipelineResult::stakePenaltyRecords() const {
  return m_stakePenaltyRecords;
}

const CryptographicSlashingSummary &
RuntimeBlockPipelineResult::cryptographicSlashingSummary() const {
  return m_cryptographicSlashingSummary;
}

const GovernancePolicySnapshot &
RuntimeBlockPipelineResult::governancePolicySnapshot() const {
  return m_governancePolicySnapshot;
}

const std::vector<GovernanceActionGuard> &
RuntimeBlockPipelineResult::governanceActionGuards() const {
  return m_governanceActionGuards;
}

const GovernanceSummary &RuntimeBlockPipelineResult::governanceSummary() const {
  return m_governanceSummary;
}

const std::vector<ValidatorLifecycleRecord> &
RuntimeBlockPipelineResult::validatorLifecycleRecords() const {
  return m_validatorLifecycleRecords;
}

const EpochAccountingRecord &
RuntimeBlockPipelineResult::epochAccountingRecord() const {
  return m_epochAccountingRecord;
}

const ValidatorLifecycleSummary &
RuntimeBlockPipelineResult::validatorLifecycleSummary() const {
  return m_validatorLifecycleSummary;
}

const economics::SupplyDelta &RuntimeBlockPipelineResult::supplyDelta() const {
  return m_supplyDelta;
}

const std::vector<economics::TreasuryExecutionEvidence> &
RuntimeBlockPipelineResult::treasuryExecutionEvidence() const {
  return m_treasuryExecutionEvidence;
}

const std::string &RuntimeBlockPipelineResult::snapshotDigest() const {
  return m_snapshotDigest;
}

const std::string &RuntimeBlockPipelineResult::receiptsRoot() const {
  return m_receiptsRoot;
}

std::string RuntimeBlockPipelineResult::serialize() const {
  std::ostringstream oss;

  oss << "RuntimeBlockPipelineResult{"
      << "status=" << runtimeBlockPipelineStatusToString(m_status)
      << ";reason=" << m_reason << ";blockHash="
      << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "NONE")
      << ";finalizedTransactionCount=" << m_finalizedTransactionIds.size()
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
      << m_validatorLifecycleSummary.status()
      << ";treasuryExecutionEvidenceCount="
      << m_treasuryExecutionEvidence.size() << "}";

  return oss.str();
}

RuntimeBlockPipelineResult RuntimeBlockPipeline::commitCertifiedBlock(
    NodeRuntime &runtime, const core::Block &block,
    const consensus::QuorumCertificate &certificate, std::int64_t finalizedAt,
    const NodeDataDirectoryConfig *directoryConfig) {
  NodeRuntime stagedRuntime = runtime;
  RuntimeBlockPipelineResult result =
      applyCertifiedBlock(stagedRuntime, block, certificate, finalizedAt);

  if (!result.finalized()) {
    return result;
  }

  if (directoryConfig != nullptr) {
    const FinalizedBlockStoreResult persisted = FinalizedBlockStore::persist(
        *directoryConfig, stagedRuntime, result, finalizedAt);

    if (!persisted.success()) {
      return RuntimeBlockPipelineResult::rejected(
          RuntimeBlockPipelineStatus::PERSISTENCE_FAILED,
          "Canonical block persistence failed: " + persisted.reason());
    }
  }

  runtime = std::move(stagedRuntime);
  return result;
}

RuntimeBlockPipelineResult RuntimeBlockPipeline::applyCertifiedBlock(
    NodeRuntime &runtime, const core::Block &block,
    const consensus::QuorumCertificate &certificate, std::int64_t finalizedAt) {
  if (!runtime.isValid()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_RUNTIME,
        "Node runtime is invalid.");
  }

  std::string epochRewardRejection;
  if (!EpochRewardSettlementService::candidateRecordsMatch(
          runtime, block, epochRewardRejection)) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        "Epoch reward validation failed: " + epochRewardRejection);
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          runtime.config().genesisConfig().networkParameters().networkName());

  if (!cryptoContext.isValid()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Protocol crypto context is invalid: " +
            cryptoContext.rejectionReason());
  }

  core::BlockValidationResult transitionValidation;
  try {
    transitionValidation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            runtime.blockchain(), block,
            RuntimeAccountStateBuilder::previewContextAtTip(
                runtime.config().genesisConfig(), runtime.blockchain(),
                minimumFeeRawUnitsForRuntime(runtime)),
            core::BlockValidationMode::StructuralOnly);
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED, error.what());
  }

  if (!transitionValidation.accepted()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        transitionValidation.reason());
  }

  const FeeEconomicBalance preMintFeeBalance =
      FeeEconomics::buildFeeEconomicBalance(block.index(),
                                            transitionValidation.totalFee());
  const RuntimeMonetaryValidationResult monetaryValidationResult =
      RuntimeMonetaryValidation::validateCandidate(
          runtime.config().genesisConfig(), block,
          preMintFeeBalance.burnAmount(), runtime.supplyState().latestSupply());

  if (!monetaryValidationResult.isAccepted()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::MONETARY_VALIDATION_FAILED,
        "Monetary gate rejected certified block: " +
            monetaryValidationResult.reason());
  }

  std::shared_ptr<ProtocolExecutionState> executionTracker;
  std::map<std::string, GovernanceProposalStatus> governanceStatusesBeforeBlock;
  try {
    auto [protocolContext, tracker] =
        ProtocolStateTransition::contextForNextBlockWithState(
            runtime, minimumFeeRawUnitsForRuntime(runtime), finalizedAt);
    executionTracker = tracker;
    governanceStatusesBeforeBlock =
        TreasuryExecutionEvidenceBuilder::snapshotStatuses(
            executionTracker->governance);
    transitionValidation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            runtime.blockchain(), block, std::move(protocolContext),
            core::BlockValidationMode::ProtocolCommitment);
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED, error.what());
  }

  if (!transitionValidation.accepted()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        transitionValidation.reason());
  }

  std::vector<RewardDistribution> rewardDistributions;
  std::vector<LockedStakePosition> lockedStakePositions;
  std::vector<SecurityScoreRecord> securityScoreRecords;
  std::vector<ValidatorSecurityCheckpoint> securityCheckpoints;
  std::vector<ValidatorRiskAssessment> validatorRiskAssessments;
  std::vector<ValidatorContainmentDecision> validatorContainmentDecisions;
  std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies;
  MonetaryFirewallAudit monetaryFirewallAudit;
  GenesisTreasurySnapshot genesisTreasurySnapshot;
  ProtectionRewardBudget protectionRewardBudget;
  std::vector<ProtectionRewardGrant> protectionRewardGrants;
  std::vector<ProtectionWorkRecord> protectionWorkRecords;
  ProtectionRewardSummary protectionRewardSummary;
  std::vector<ProtectionRewardSettlement> protectionRewardSettlements;
  InflationEpochSnapshot inflationEpochSnapshot;
  MintAuthorizationRecord mintAuthorizationRecord;
  SupplyExpansionRecord supplyExpansionRecord;
  FeeEconomicBalance feeEconomicBalance;
  FeeBurnRecord feeBurnRecord;
  TreasuryFeeRecord treasuryFeeRecord;
  std::vector<SlashingEvidenceRecord> slashingEvidenceRecords;
  std::vector<SlashingPreparationRecord> slashingPreparationRecords;
  SlashingEvidenceSummary slashingEvidenceSummary;
  std::vector<CryptographicSlashingEvidenceRecord>
      cryptographicSlashingEvidenceRecords;
  std::vector<StakePenaltyRecord> stakePenaltyRecords;
  CryptographicSlashingSummary cryptographicSlashingSummary;
  GovernancePolicySnapshot governancePolicySnapshot;
  std::vector<GovernanceActionGuard> governanceActionGuards;
  GovernanceSummary governanceSummary;
  std::vector<economics::TreasuryExecutionEvidence> treasuryExecutionEvidence;

  try {
    feeEconomicBalance = FeeEconomics::buildFeeEconomicBalance(
        block.index(), transitionValidation.totalFee());
    rewardDistributions =
        RewardDistributionCalculator::buildFromQuorumCertificate(
            feeEconomicBalance.validatorRewardAmount(), certificate,
            block.index());
    lockedStakePositions =
        LockedStakePositionBuilder::buildFromRewardDistributions(
            rewardDistributions);
    securityScoreRecords =
        SecurityScoreCalculator::buildFromLockedStakePositions(
            lockedStakePositions, block.index());
    securityCheckpoints =
        ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
            securityScoreRecords, lockedStakePositions, block.index());
    validatorRiskAssessments =
        ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
            securityCheckpoints);
    validatorContainmentDecisions =
        ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
            validatorRiskAssessments);
    validatorNetworkPolicies =
        ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
            validatorContainmentDecisions);
    feeBurnRecord = FeeEconomics::buildFeeBurnRecord(
        feeEconomicBalance,
        monetaryValidationResult.supplyDelta().supplyBefore());
    treasuryFeeRecord =
        FeeEconomics::buildTreasuryFeeRecord(feeEconomicBalance);
    monetaryFirewallAudit =
        monetaryValidationResult.supplyDelta().mintedAmount().isPositive()
            ? MonetaryFirewall::buildEpochRewardAuditWithSupplyBefore(
                  block.index(),
                  monetaryValidationResult.supplyDelta().supplyBefore(),
                  monetaryValidationResult.supplyDelta().mintedAmount(),
                  monetaryValidationResult.supplyDelta().burnedAmount(),
                  treasuryFeeRecord.treasuryAmount(), utils::Amount())
            : MonetaryFirewall::buildAuditWithSupplyBefore(
                  block.index(),
                  monetaryValidationResult.supplyDelta().supplyBefore(),
                  utils::Amount(),
                  monetaryValidationResult.supplyDelta().burnedAmount(),
                  treasuryFeeRecord.treasuryAmount(), utils::Amount());

    if (!monetaryFirewallAudit.passed()) {
      throw std::runtime_error("Monetary firewall audit did not pass.");
    }

    genesisTreasurySnapshot = ProtectionTreasury::buildGenesisTreasurySnapshot(
        runtime.config().genesisConfig(), block.index(),
        treasuryFeeRecord.treasuryAmount());
    protectionRewardBudget = ProtectionTreasury::buildProtectionRewardBudget(
        genesisTreasurySnapshot, rewardDistributions);
    protectionRewardGrants = ProtectionTreasury::buildProtectionRewardGrants(
        protectionRewardBudget, rewardDistributions, securityScoreRecords);
    protectionWorkRecords = ProtectionRewards::buildWorkRecords(
        protectionRewardGrants, securityScoreRecords, validatorRiskAssessments,
        validatorNetworkPolicies);
    protectionRewardSettlements = ProtectionRewards::buildSettlements(
        protectionRewardGrants, protectionWorkRecords);
    protectionRewardSummary = ProtectionRewards::buildSummary(
        protectionRewardBudget, protectionRewardSettlements);
    inflationEpochSnapshot = ControlledIssuance::buildInflationEpochSnapshot(
        runtime.config().genesisConfig(), block.index(),
        monetaryFirewallAudit.annualMintUsedAfter());
    if (monetaryValidationResult.supplyDelta().mintedAmount().isPositive()) {
      const auto &mints = monetaryValidationResult.supplyDelta().mintRecords();
      if (mints.empty()) {
        throw std::logic_error("Minted supply has no canonical mint records.");
      }
      std::string rewardEvidenceDigest;
      for (const auto &ledgerRecord : block.records()) {
        if (ledgerRecord.type() == core::LedgerRecordType::PROTECTION_EPOCH) {
          rewardEvidenceDigest = ledgerRecord.payloadHash();
          break;
        }
      }
      mintAuthorizationRecord =
          ControlledIssuance::buildEpochRewardAuthorization(
              inflationEpochSnapshot,
              monetaryValidationResult.supplyDelta().mintedAmount(),
              mints.front().authorizationId(), rewardEvidenceDigest);
      supplyExpansionRecord = ControlledIssuance::buildEpochRewardExpansion(
          mintAuthorizationRecord, inflationEpochSnapshot);
    } else {
      mintAuthorizationRecord =
          ControlledIssuance::buildNoMintAuthorization(inflationEpochSnapshot);
      supplyExpansionRecord = ControlledIssuance::buildNoSupplyExpansion(
          mintAuthorizationRecord, inflationEpochSnapshot);
    }
    slashingEvidenceRecords = SlashingEvidence::buildEvidenceRecords(
        validatorRiskAssessments, validatorNetworkPolicies,
        protectionWorkRecords);
    slashingPreparationRecords = SlashingEvidence::buildPreparationRecords(
        slashingEvidenceRecords, lockedStakePositions);
    slashingEvidenceSummary = SlashingEvidence::buildSummary(
        block.index(), slashingEvidenceRecords, slashingPreparationRecords);
    cryptographicSlashingEvidenceRecords =
        CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
            certificate.votes());
    stakePenaltyRecords = CryptographicSlashing::buildStakePenaltyRecords(
        cryptographicSlashingEvidenceRecords, lockedStakePositions);
    cryptographicSlashingSummary = CryptographicSlashing::buildSummary(
        block.index(), cryptographicSlashingEvidenceRecords,
        stakePenaltyRecords);
    governancePolicySnapshot = Governance::buildPolicySnapshot(block.index());
    governanceActionGuards =
        Governance::buildActionGuards(governancePolicySnapshot);
    if (!executionTracker) {
      throw std::runtime_error("Missing governance execution tracker.");
    }
    governanceSummary = Governance::buildSummary(
        block.index(), governanceActionGuards,
        static_cast<std::uint64_t>(
            executionTracker->governance.activeProposalCount()),
        static_cast<std::uint64_t>(
            executionTracker->governance.approvedProposalCount()),
        static_cast<std::uint64_t>(
            executionTracker->governance.executableProposalCount(block.index() +
                                                                 1)),
        static_cast<std::uint64_t>(
            executionTracker->governance.executedProposalCount()),
        executionTracker->governance.serialize());
    treasuryExecutionEvidence =
        TreasuryExecutionEvidenceBuilder::buildForNewlyExecuted(
            governanceStatusesBeforeBlock, executionTracker->governance,
            runtime.config().genesisConfig().networkParameters());
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        std::string("Economic accounting failed: ") + error.what());
  }

  try {
    RuntimeSupplyState supplyStateProbe = runtime.supplyState();
    supplyStateProbe.applyFinalizedDelta(
        monetaryValidationResult.supplyDelta());
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        std::string("Supply continuity check failed: ") + error.what());
  }

  if (!runtime.validatorSetHistory().hasSet(block.index())) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
        "Validator set history is missing for the finalized block height.");
  }

  const core::ValidatorRegistry &finalizingValidatorSet =
      runtime.validatorSetHistory().setAt(block.index());

  const consensus::BlockFinalizationResult finalization =
      consensus::BlockFinalizer::finalizeBlock(
          runtime.mutableBlockchain(), block, certificate,
          finalizingValidatorSet, runtime.mutableFinalizationRegistry(),
          cryptoContext.policy(), cryptoContext.signatureProvider(),
          finalizedAt);

  if (!finalization.finalized()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
        finalization.duplicate() ? "Certified block was already finalized."
                                 : finalization.reason());
  }

  std::vector<std::string> finalizedTransactionIds;
  for (const core::LedgerRecord &record : block.records()) {
    if (record.type() == core::LedgerRecordType::TRANSACTION) {
      finalizedTransactionIds.push_back(record.sourceId());
    }
  }

  RuntimeBlockPipelineResult finalResult =
      RuntimeBlockPipelineResult::finalized(
          block, certificate, finalization.record(), finalizedTransactionIds,
          transitionValidation.stateRoot(), transitionValidation.totalFee(),
          rewardDistributions, lockedStakePositions, securityScoreRecords,
          securityCheckpoints, validatorRiskAssessments,
          validatorContainmentDecisions, validatorNetworkPolicies,
          monetaryFirewallAudit, genesisTreasurySnapshot,
          protectionRewardBudget, protectionRewardGrants, protectionWorkRecords,
          protectionRewardSummary, protectionRewardSettlements,
          inflationEpochSnapshot, mintAuthorizationRecord,
          supplyExpansionRecord, feeEconomicBalance, feeBurnRecord,
          treasuryFeeRecord, slashingEvidenceRecords,
          slashingPreparationRecords, slashingEvidenceSummary,
          cryptographicSlashingEvidenceRecords, stakePenaltyRecords,
          cryptographicSlashingSummary, governancePolicySnapshot,
          governanceActionGuards, governanceSummary,
          monetaryValidationResult.supplyDelta(), treasuryExecutionEvidence);

  try {
    runtime.mutableSupplyState().applyFinalizedDelta(
        monetaryValidationResult.supplyDelta());
    if (!executionTracker ||
        executionTracker->supply != runtime.supplyState().latestSupply()) {
      throw std::logic_error(
          "Canonical transaction execution and monetary supply diverged.");
    }
    runtime.mutableGovernanceExecutor() = executionTracker->governance;
    runtime.mutableValidatorRegistry() = executionTracker->validators;
    runtime.mutableValidatorPenaltyLedger() = executionTracker->penaltyLedger;
    runtime.mutableStakingRegistry() = executionTracker->staking;

    const FinalizedSlashingEvidenceAuditResult slashingAudit =
        FinalizedSlashingEvidenceAudit::auditBlockEffects(
            block, runtime.validatorPenaltyLedger(),
            runtime.validatorRegistry(), runtime.stakingRegistry());
    if (!slashingAudit.passed()) {
      throw std::logic_error("Finalized slashing evidence audit failed: " +
                             slashingAudit.reason());
    }

    removeFinalizedTransactionsFromMempool(runtime, finalizedTransactionIds);
    runtime.invalidateAccountStateCache();

    finalResult.m_receiptsRoot = transitionValidation.receiptsRoot();

    if (!finalResult.postStateRoot().empty()) {
      runtime.mutableStatePruner().recordStateRoot(block.index(),
                                                   finalResult.postStateRoot());
      runtime.mutableStatePruner().pruneHistory(block.index());
    }

    if (block.index() > 0 && block.index() % NODO_VALIDATOR_EPOCH_BLOCKS == 0) {
      const std::int64_t boundaryTimestamp = block.timestamp();
      try {
        const FastSyncSnapshot epochSnapshot = FastSyncSnapshot::fromRuntime(
            runtime, boundaryTimestamp);
        finalResult.m_snapshotDigest = epochSnapshot.digest();
      } catch (...) {
        // The canonical state remains valid without this derived cache.
      }
    }

    constexpr std::uint64_t nextRound = 1;
    const std::uint64_t nextHeight = block.index() + 1;
    if (!runtime.mutableValidatorSetHistory().recordSet(
            nextHeight, runtime.validatorRegistry())) {
      throw std::logic_error(
          "Validator set history conflicts at the next consensus height.");
    }
    const std::string nextProposer =
        consensus::ProposerSchedule::selectProposer(
            runtime.validatorRegistry(),
            runtime.config().genesisConfig().networkParameters().chainId(),
            nextHeight, nextRound);
    runtime.mutableConsensusRoundManager().advanceToHeight(
        nextHeight, nextRound, nextProposer, finalizedAt + 1,
        runtime.config()
            .genesisConfig()
            .networkParameters()
            .targetBlockTimeSeconds());
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        std::string("Canonical post-quorum commit failed: ") + error.what());
  }

  return finalResult;
}

RuntimeBlockPipelineResult
RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
    NodeRuntime &runtime, const RuntimeBlockPipelineConfig &config,
    const crypto::Signer &localValidatorSigner,
    const NodeDataDirectoryConfig *directoryConfig) {
  if (!config.isValid()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Runtime block pipeline config is invalid.");
  }

  if (!runtime.isValid()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_RUNTIME,
        "Node runtime is invalid.");
  }

  if (runtime.config().genesisConfig().networkParameters().networkClass() !=
      config::NetworkClass::DEVELOPMENT_LOCAL) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Local block production/finalization helper is restricted to "
        "DEVELOPMENT_LOCAL networks. "
        "Staging and production networks must finalize through distributed "
        "PREVOTE/PRECOMMIT consensus.");
  }

  const consensus::ConsensusRoundState activeRound =
      runtime.consensusRoundManager().currentState();

  if (config.consensusRound() != activeRound.round()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
        "Consensus round mismatch: pipeline requested round " +
            std::to_string(config.consensusRound()) +
            " but runtime is at round " + std::to_string(activeRound.round()) +
            ".");
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          runtime.config().genesisConfig().networkParameters().networkName());

  if (!cryptoContext.isValid()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Protocol crypto context is invalid for network '" +
            runtime.config().genesisConfig().networkParameters().networkName() +
            "': " + cryptoContext.rejectionReason());
  }

  const consensus::BlockCandidateResult production =
      consensus::BlockProductionPhase::produce(runtime, config);

  if (!production.produced()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED,
        production.reason());
  }

  core::Block candidateBlock = production.block();

  if (candidateBlock.index() != activeRound.height()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
        "Candidate block height " + std::to_string(candidateBlock.index()) +
            " does not match active consensus height " +
            std::to_string(activeRound.height()) + ".");
  }

  core::BlockValidationResult transitionValidation;

  try {
    transitionValidation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            runtime.blockchain(), candidateBlock,
            previewContextForRuntime(runtime, config.timestamp()));
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED, error.what());
  }

  if (!transitionValidation.accepted()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        transitionValidation.reason());
  }

  // Pre-vote monetary gate: the candidate must pass monetary validation before
  // any validator votes are built. MONETARY_CONTEXT_UNAVAILABLE is also a
  // rejection; it is never treated as an implicit success.
  // The validated SupplyDelta is preserved and propagated into the finalized
  // result.
  const FeeEconomicBalance preMintFeeBalance =
      FeeEconomics::buildFeeEconomicBalance(candidateBlock.index(),
                                            transitionValidation.totalFee());

  const RuntimeMonetaryValidationResult monetaryValidationResult =
      RuntimeMonetaryValidation::validateCandidate(
          runtime.config().genesisConfig(), candidateBlock,
          preMintFeeBalance.burnAmount(), runtime.supplyState().latestSupply());

  if (!monetaryValidationResult.isAccepted()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::MONETARY_VALIDATION_FAILED,
        "Monetary gate rejected candidate block: " +
            monetaryValidationResult.reason());
  }

  // The block state commitment includes the post-transition monetary supply.
  // Rebuild the candidate before voting so the QC signs the complete state,
  // not an accounts-only or pre-supply root.
  try {
    const core::StateTransitionPreviewContext committedContext =
        RuntimeAccountStateBuilder::previewContextAtTip(
            runtime, minimumFeeRawUnitsForRuntime(runtime));
    const core::Block draft(
        candidateBlock.index(), candidateBlock.previousHash(),
        candidateBlock.records(), candidateBlock.timestamp(), "", "");
    const core::StateTransitionPreviewResult committedPreview =
        core::StateTransitionEngine::executeBlock(draft, committedContext);
    if (!committedPreview.accepted()) {
      return RuntimeBlockPipelineResult::rejected(
          RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
          "Unable to compute complete post-state commitment: " +
              committedPreview.reason());
    }
    candidateBlock = core::Block(
        draft.index(), draft.previousHash(), draft.records(), draft.timestamp(),
        committedPreview.stateRoot(), committedPreview.receiptsRoot());
    transitionValidation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            runtime.blockchain(), candidateBlock, committedContext);
    if (!transitionValidation.accepted()) {
      return RuntimeBlockPipelineResult::rejected(
          RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
          transitionValidation.reason());
    }
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED, error.what());
  }

  std::vector<consensus::ValidatorVoteRecord> votes;

  try {
    votes = buildLocalnetPrecommitVotes(
        runtime, candidateBlock, activeRound.round(), config.timestamp() + 1,
        localValidatorSigner);
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED, error.what());
  }

  if (votes.empty()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS,
        "No active validators are available to vote.");
  }

  for (const consensus::ValidatorVoteRecord &vote : votes) {
    const consensus::VoteCollectResult collected =
        runtime.submitConsensusVote(vote);

    if (!collected.accepted()) {
      return RuntimeBlockPipelineResult::rejected(
          RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
          "Consensus vote rejected by active round manager: " +
              consensus::voteCollectStatusToString(collected.status()) + ": " +
              collected.reason());
    }
  }

  const consensus::QuorumCertificateBuildResult certificate =
      consensus::QuorumCertificateBuilder::buildFromVotes(
          candidateBlock.index(), candidateBlock.hash(),
          candidateBlock.previousHash(), activeRound.round(), votes,
          runtime.validatorSetHistory().setAt(candidateBlock.index()),
          cryptoContext.policy(), cryptoContext.signatureProvider(),
          runtime.config()
              .genesisConfig()
              .networkParameters()
              .quorumThresholdNumerator(),
          runtime.config()
              .genesisConfig()
              .networkParameters()
              .quorumThresholdDenominator());

  if (!certificate.certified()) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED, certificate.reason());
  }

  return commitCertifiedBlock(runtime, candidateBlock,
                              certificate.certificate(), config.timestamp() + 2,
                              directoryConfig);
}

std::vector<consensus::ValidatorVoteRecord>
RuntimeBlockPipeline::buildLocalnetPrecommitVotes(
    const NodeRuntime &runtime, const core::Block &block,
    std::uint64_t consensusRound, std::int64_t timestamp,
    const crypto::Signer &localValidatorSigner) {
  std::vector<consensus::ValidatorVoteRecord> votes;

  if (!runtime.validatorSetHistory().hasSet(block.index())) {
    throw std::runtime_error(
        "Validator set history is missing for vote height.");
  }

  votes.push_back(consensus::ValidatorVoteBuilder::buildPrecommit(
      runtime.validatorSetHistory().setAt(block.index()), block, consensusRound,
      timestamp, localValidatorSigner));

  return votes;
}

void RuntimeBlockPipeline::removeFinalizedTransactionsFromMempool(
    NodeRuntime &runtime, const std::vector<std::string> &transactionIds) {
  for (const std::string &transactionId : transactionIds) {
    runtime.mutableMempool().removeTransaction(transactionId);
  }
}

} // namespace nodo::node
