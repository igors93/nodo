#include "node/RuntimeBlockPipeline.hpp"

#include "consensus/ValidatorVoteBuilder.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/State.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnitsForRuntime(
    const NodeRuntime& runtime
) {
    const std::uint64_t minimumFee =
        runtime.config().genesisConfig().networkParameters().minimumFeeRawUnits();

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(minimumFee);
}

core::StateTransitionPreviewContext previewContextForRuntime(
    const NodeRuntime& runtime
) {
    const std::int64_t minimumFee =
        minimumFeeRawUnitsForRuntime(runtime);

    return RuntimeAccountStateBuilder::previewContextAtTip(
        runtime.config().genesisConfig(),
        runtime.blockchain(),
        minimumFee
    );
}

bool rewardDistributionsMatchValidatorReward(
    utils::Amount validatorReward,
    const std::vector<RewardDistribution>& rewardDistributions
) {
    try {
        if (validatorReward.isZero()) {
            return rewardDistributions.empty();
        }

        return RewardDistributionCalculator::totalReward(
            rewardDistributions
        ) == validatorReward;
    } catch (const std::exception&) {
        return false;
    }
}

bool lockedStakePositionsMatchRewards(
    const std::vector<RewardDistribution>& rewardDistributions,
    const std::vector<LockedStakePosition>& lockedStakePositions
) {
    try {
        return LockedStakePositionBuilder::samePositions(
            LockedStakePositionBuilder::buildFromRewardDistributions(
                rewardDistributions
            ),
            lockedStakePositions
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool securityScoreRecordsMatchLockedStake(
    const std::vector<LockedStakePosition>& lockedStakePositions,
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    std::uint64_t blockHeight
) {
    try {
        return SecurityScoreCalculator::sameRecords(
            SecurityScoreCalculator::buildFromLockedStakePositions(
                lockedStakePositions,
                blockHeight
            ),
            securityScoreRecords
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool securityCheckpointsMatchScores(
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<LockedStakePosition>& lockedStakePositions,
    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints,
    std::uint64_t blockHeight
) {
    try {
        return ValidatorSecurityCheckpointBuilder::sameCheckpoints(
            ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
                securityScoreRecords,
                lockedStakePositions,
                blockHeight
            ),
            securityCheckpoints
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool validatorRiskAssessmentsMatchCheckpoints(
    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints,
    const std::vector<ValidatorRiskAssessment>& validatorRiskAssessments
) {
    try {
        return ValidatorRiskAssessmentBuilder::sameAssessments(
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
                securityCheckpoints
            ),
            validatorRiskAssessments
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool validatorContainmentDecisionsMatchRisk(
    const std::vector<ValidatorRiskAssessment>& validatorRiskAssessments,
    const std::vector<ValidatorContainmentDecision>& validatorContainmentDecisions
) {
    try {
        return ValidatorContainmentDecisionBuilder::sameDecisions(
            ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
                validatorRiskAssessments
            ),
            validatorContainmentDecisions
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool validatorNetworkPoliciesMatchContainment(
    const std::vector<ValidatorContainmentDecision>& validatorContainmentDecisions,
    const std::vector<ValidatorNetworkPolicy>& validatorNetworkPolicies
) {
    try {
        return ValidatorNetworkPolicyBuilder::samePolicies(
            ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
                validatorContainmentDecisions
            ),
            validatorNetworkPolicies
        );
    } catch (const std::exception&) {
        return false;
    }
}

bool monetaryFirewallAuditIsStructurallyValid(
    const MonetaryFirewallAudit& audit
) {
    return audit.isValid();
}

bool protectionTreasuryPlanIsValid(
    const GenesisTreasurySnapshot& treasurySnapshot,
    const ProtectionRewardBudget& budget,
    const std::vector<ProtectionRewardGrant>& grants,
    const std::vector<RewardDistribution>& rewardDistributions,
    const std::vector<SecurityScoreRecord>& securityScoreRecords
) {
    try {
        if (treasurySnapshot.status() == "NOT_EVALUATED" &&
            budget.status() == "NOT_EVALUATED" &&
            grants.empty()) {
            return treasurySnapshot.isValid() && budget.isValid();
        }

        return treasurySnapshot.active() &&
               budget.active() &&
               ProtectionTreasury::sameBudget(
                   ProtectionTreasury::buildProtectionRewardBudget(
                       treasurySnapshot,
                       rewardDistributions
                   ),
                   budget
               ) &&
               ProtectionTreasury::sameGrants(
                   ProtectionTreasury::buildProtectionRewardGrants(
                       budget,
                       rewardDistributions,
                       securityScoreRecords
                   ),
                   grants
               );
    } catch (const std::exception&) {
        return false;
    }
}
bool protectionRewardsPlanIsValid(
    const ProtectionRewardBudget& budget,
    const std::vector<ProtectionRewardGrant>& grants,
    const std::vector<ProtectionWorkRecord>& workRecords,
    const ProtectionRewardSummary& summary,
    const std::vector<ProtectionRewardSettlement>& settlements,
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<ValidatorRiskAssessment>& riskAssessments,
    const std::vector<ValidatorNetworkPolicy>& networkPolicies
) {
    try {
        return summary.active() &&
               ProtectionRewards::sameWorkRecords(
                   ProtectionRewards::buildWorkRecords(
                       grants,
                       securityScoreRecords,
                       riskAssessments,
                       networkPolicies
                   ),
                   workRecords
               ) &&
               ProtectionRewards::sameSettlements(
                   ProtectionRewards::buildSettlements(
                       grants,
                       workRecords
                   ),
                   settlements
               ) &&
               ProtectionRewards::sameSummary(
                   ProtectionRewards::buildSummary(
                       budget,
                       settlements
                   ),
                   summary
               );
    } catch (const std::exception&) {
        return false;
    }
}

bool slashingEvidencePlanIsValid(
    std::uint64_t blockHeight,
    const std::vector<ValidatorRiskAssessment>& riskAssessments,
    const std::vector<ValidatorNetworkPolicy>& networkPolicies,
    const std::vector<ProtectionWorkRecord>& protectionWorkRecords,
    const std::vector<LockedStakePosition>& lockedStakePositions,
    const std::vector<SlashingEvidenceRecord>& evidenceRecords,
    const std::vector<SlashingPreparationRecord>& preparationRecords,
    const SlashingEvidenceSummary& summary
) {
    try {
        const std::vector<SlashingEvidenceRecord> expectedEvidence =
            SlashingEvidence::buildEvidenceRecords(
                riskAssessments,
                networkPolicies,
                protectionWorkRecords
            );

        const std::vector<SlashingPreparationRecord> expectedPreparations =
            SlashingEvidence::buildPreparationRecords(
                expectedEvidence,
                lockedStakePositions
            );

        const SlashingEvidenceSummary expectedSummary =
            SlashingEvidence::buildSummary(
                blockHeight,
                expectedEvidence,
                expectedPreparations
            );

        return SlashingEvidence::sameEvidenceRecords(expectedEvidence, evidenceRecords) &&
               SlashingEvidence::samePreparationRecords(expectedPreparations, preparationRecords) &&
               SlashingEvidence::sameSummary(expectedSummary, summary);
    } catch (const std::exception&) {
        return false;
    }
}

bool controlledIssuancePlanIsValid(
    const InflationEpochSnapshot& epoch,
    const MintAuthorizationRecord& authorization,
    const SupplyExpansionRecord& expansion
) {
    try {
        if (epoch.status() == "NOT_EVALUATED") {
            return false;
        }

        return epoch.active() &&
               authorization.isValid() &&
               expansion.isValid() &&
               ControlledIssuance::sameAuthorization(
                   ControlledIssuance::buildNoMintAuthorization(epoch),
                   authorization
               ) &&
               ControlledIssuance::sameExpansion(
                   ControlledIssuance::buildNoSupplyExpansion(authorization, epoch),
                   expansion
               );
    } catch (const std::exception&) {
        return false;
    }
}
bool feeEconomicsPlanIsValid(
    utils::Amount totalFee,
    const FeeEconomicBalance& feeBalance,
    const FeeBurnRecord& burnRecord,
    const TreasuryFeeRecord& treasuryRecord
) {
    try {
        return feeBalance.active() &&
               burnRecord.active() &&
               treasuryRecord.active() &&
               feeBalance.totalFee() == totalFee &&
               FeeEconomics::sameBalance(
                   FeeEconomics::buildFeeEconomicBalance(
                       feeBalance.blockHeight(),
                       totalFee
                   ),
                   feeBalance
               ) &&
               FeeEconomics::sameBurn(
                   FeeEconomics::buildFeeBurnRecord(
                       feeBalance,
                       burnRecord.supplyBefore()
                   ),
                   burnRecord
               ) &&
               FeeEconomics::sameTreasuryFee(
                   FeeEconomics::buildTreasuryFeeRecord(
                       feeBalance
                   ),
                   treasuryRecord
               );
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig()
    : m_maxTransactionsPerBlock(1000),
      m_minTransactionsPerBlock(1),
      m_consensusRound(1),
      m_timestamp(0) {}

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig(
    std::size_t maxTransactionsPerBlock,
    std::size_t minTransactionsPerBlock,
    std::uint64_t consensusRound,
    std::int64_t timestamp
)
    : m_maxTransactionsPerBlock(maxTransactionsPerBlock),
      m_minTransactionsPerBlock(minTransactionsPerBlock),
      m_consensusRound(consensusRound),
      m_timestamp(timestamp) {}

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
           m_minTransactionsPerBlock > 0 &&
           m_minTransactionsPerBlock <= m_maxTransactionsPerBlock &&
           m_consensusRound > 0 &&
           m_timestamp > 0;
}

std::string RuntimeBlockPipelineConfig::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeBlockPipelineConfig{"
        << "maxTransactionsPerBlock=" << m_maxTransactionsPerBlock
        << ";minTransactionsPerBlock=" << m_minTransactionsPerBlock
        << ";consensusRound=" << m_consensusRound
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

std::string runtimeBlockPipelineStatusToString(
    RuntimeBlockPipelineStatus status
) {
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
        case RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS:
            return "NOT_ENOUGH_VALIDATORS";
        case RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED:
            return "VOTE_BUILD_FAILED";
        case RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED:
            return "QUORUM_BUILD_FAILED";
        case RuntimeBlockPipelineStatus::FINALIZATION_FAILED:
            return "FINALIZATION_FAILED";
        default:
            return "FINALIZATION_FAILED";
    }
}

RuntimeBlockPipelineResult::RuntimeBlockPipelineResult()
    : m_status(RuntimeBlockPipelineStatus::INVALID_CONFIG),
      m_reason("Uninitialized runtime block pipeline result."),
      m_block(std::nullopt),
      m_certificate(),
      m_finalizedRecord(),
      m_finalizedTransactionIds(),
      m_postStateRoot(""),
      m_totalFee(),
      m_rewardDistributions(),
      m_lockedStakePositions(),
      m_securityScoreRecords(),
      m_securityCheckpoints(),
      m_validatorRiskAssessments(),
      m_validatorContainmentDecisions(),
      m_validatorNetworkPolicies(),
      m_monetaryFirewallAudit(MonetaryFirewallAudit::notEvaluated()),
      m_genesisTreasurySnapshot(GenesisTreasurySnapshot::notEvaluated()),
      m_protectionRewardBudget(ProtectionRewardBudget::notEvaluated()),
      m_protectionRewardGrants(),
      m_protectionWorkRecords(),
      m_protectionRewardSummary(ProtectionRewardSummary::notEvaluated()),
      m_protectionRewardSettlements(),
      m_inflationEpochSnapshot(InflationEpochSnapshot::notEvaluated()),
      m_mintAuthorizationRecord(),
      m_supplyExpansionRecord(),
      m_feeEconomicBalance(FeeEconomicBalance::notEvaluated()),
      m_feeBurnRecord(FeeBurnRecord::notEvaluated()),
      m_treasuryFeeRecord(TreasuryFeeRecord::notEvaluated()),
      m_slashingEvidenceRecords(),
      m_slashingPreparationRecords(),
      m_slashingEvidenceSummary(SlashingEvidenceSummary::notEvaluated()) {}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot
) {
    return finalized(std::move(block), std::move(certificate), std::move(finalizedRecord),
        std::move(finalizedTransactionIds), std::move(postStateRoot), utils::Amount(), {}, {}, {}, {}, {}, {}, {});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee
) {
    return finalized(std::move(block), std::move(certificate), std::move(finalizedRecord),
        std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee, {}, {}, {}, {}, {}, {}, {});
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions
) {
    std::vector<LockedStakePosition> lockedStakePositions;

    try {
        lockedStakePositions =
            LockedStakePositionBuilder::buildFromRewardDistributions(rewardDistributions);
    } catch (const std::exception&) {
        lockedStakePositions.clear();
    }

    return finalized(std::move(block), std::move(certificate), std::move(finalizedRecord),
        std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
        std::move(rewardDistributions), std::move(lockedStakePositions));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions
) {
    std::vector<SecurityScoreRecord> securityScoreRecords;

    if (!lockedStakePositions.empty() && block.isValid()) {
        try {
            securityScoreRecords =
                SecurityScoreCalculator::buildFromLockedStakePositions(
                    lockedStakePositions,
                    block.index()
                );
        } catch (const std::exception&) {
            securityScoreRecords.clear();
        }
    }

    return finalized(std::move(block), std::move(certificate), std::move(finalizedRecord),
        std::move(finalizedTransactionIds), std::move(postStateRoot), totalFee,
        std::move(rewardDistributions), std::move(lockedStakePositions),
        std::move(securityScoreRecords));
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords
) {
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints;

    if (!securityScoreRecords.empty() && block.isValid()) {
        try {
            securityCheckpoints =
                ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
                    securityScoreRecords,
                    lockedStakePositions,
                    block.index()
                );
        } catch (const std::exception&) {
            securityCheckpoints.clear();
        }
    }

    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints)
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints
) {
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments;

    try {
        validatorRiskAssessments =
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
                securityCheckpoints
            );
    } catch (const std::exception&) {
        validatorRiskAssessments.clear();
    }

    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments)
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments
) {
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions;

    try {
        validatorContainmentDecisions =
            ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
                validatorRiskAssessments
            );
    } catch (const std::exception&) {
        validatorContainmentDecisions.clear();
    }

    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions)
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions
) {
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies;

    try {
        validatorNetworkPolicies =
            ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
                validatorContainmentDecisions
            );
    } catch (const std::exception&) {
        validatorNetworkPolicies.clear();
    }

    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies)
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies
) {
    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies),
        MonetaryFirewallAudit::notEvaluated()
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
    MonetaryFirewallAudit monetaryFirewallAudit
) {
    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies),
        std::move(monetaryFirewallAudit),
        GenesisTreasurySnapshot::notEvaluated(),
        ProtectionRewardBudget::notEvaluated(),
        {}
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
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
    std::vector<ProtectionRewardGrant> protectionRewardGrants
) {
    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies),
        std::move(monetaryFirewallAudit),
        std::move(genesisTreasurySnapshot),
        std::move(protectionRewardBudget),
        std::move(protectionRewardGrants),
        InflationEpochSnapshot::notEvaluated(),
        MintAuthorizationRecord(),
        SupplyExpansionRecord()
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
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
    InflationEpochSnapshot inflationEpochSnapshot,
    MintAuthorizationRecord mintAuthorizationRecord,
    SupplyExpansionRecord supplyExpansionRecord
) {
    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies),
        std::move(monetaryFirewallAudit),
        std::move(genesisTreasurySnapshot),
        std::move(protectionRewardBudget),
        std::move(protectionRewardGrants),
        {},
        ProtectionRewardSummary::notEvaluated(),
        {},
        std::move(inflationEpochSnapshot),
        std::move(mintAuthorizationRecord),
        std::move(supplyExpansionRecord),
        FeeEconomicBalance::notEvaluated(),
        FeeBurnRecord::notEvaluated(),
        TreasuryFeeRecord::notEvaluated()
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
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
    TreasuryFeeRecord treasuryFeeRecord
) {
    return finalized(
        std::move(block),
        std::move(certificate),
        std::move(finalizedRecord),
        std::move(finalizedTransactionIds),
        std::move(postStateRoot),
        totalFee,
        std::move(rewardDistributions),
        std::move(lockedStakePositions),
        std::move(securityScoreRecords),
        std::move(securityCheckpoints),
        std::move(validatorRiskAssessments),
        std::move(validatorContainmentDecisions),
        std::move(validatorNetworkPolicies),
        std::move(monetaryFirewallAudit),
        std::move(genesisTreasurySnapshot),
        std::move(protectionRewardBudget),
        std::move(protectionRewardGrants),
        std::move(protectionWorkRecords),
        std::move(protectionRewardSummary),
        std::move(protectionRewardSettlements),
        std::move(inflationEpochSnapshot),
        std::move(mintAuthorizationRecord),
        std::move(supplyExpansionRecord),
        std::move(feeEconomicBalance),
        std::move(feeBurnRecord),
        std::move(treasuryFeeRecord),
        {},
        {},
        SlashingEvidenceSummary::notEvaluated()
    );
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds,
    std::string postStateRoot,
    utils::Amount totalFee,
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
    SlashingEvidenceSummary slashingEvidenceSummary
) {
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
    result.m_validatorContainmentDecisions = std::move(validatorContainmentDecisions);
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
    return result;
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::rejected(
    RuntimeBlockPipelineStatus status,
    std::string reason
) {
    RuntimeBlockPipelineResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

RuntimeBlockPipelineStatus RuntimeBlockPipelineResult::status() const {
    return m_status;
}

const std::string& RuntimeBlockPipelineResult::reason() const {
    return m_reason;
}

bool RuntimeBlockPipelineResult::finalized() const {
    return m_status == RuntimeBlockPipelineStatus::FINALIZED &&
           m_block.has_value() &&
           m_block->isValid() &&
           m_certificate.isStructurallyValid() &&
           m_finalizedRecord.isStructurallyValid() &&
           !m_postStateRoot.empty() &&
           !m_totalFee.isNegative() &&
           feeEconomicsPlanIsValid(
               m_totalFee,
               m_feeEconomicBalance,
               m_feeBurnRecord,
               m_treasuryFeeRecord
           ) &&
           rewardDistributionsMatchValidatorReward(m_feeEconomicBalance.validatorRewardAmount(), m_rewardDistributions) &&
           lockedStakePositionsMatchRewards(m_rewardDistributions, m_lockedStakePositions) &&
           securityScoreRecordsMatchLockedStake(m_lockedStakePositions, m_securityScoreRecords, m_block->index()) &&
           securityCheckpointsMatchScores(m_securityScoreRecords, m_lockedStakePositions, m_securityCheckpoints, m_block->index()) &&
           validatorRiskAssessmentsMatchCheckpoints(m_securityCheckpoints, m_validatorRiskAssessments) &&
           validatorContainmentDecisionsMatchRisk(m_validatorRiskAssessments, m_validatorContainmentDecisions) &&
           validatorNetworkPoliciesMatchContainment(m_validatorContainmentDecisions, m_validatorNetworkPolicies) &&
           monetaryFirewallAuditIsStructurallyValid(m_monetaryFirewallAudit) &&
           protectionTreasuryPlanIsValid(
               m_genesisTreasurySnapshot,
               m_protectionRewardBudget,
               m_protectionRewardGrants,
               m_rewardDistributions,
               m_securityScoreRecords
           ) &&
           protectionRewardsPlanIsValid(
               m_protectionRewardBudget,
               m_protectionRewardGrants,
               m_protectionWorkRecords,
               m_protectionRewardSummary,
               m_protectionRewardSettlements,
               m_securityScoreRecords,
               m_validatorRiskAssessments,
               m_validatorNetworkPolicies
           ) &&
           controlledIssuancePlanIsValid(
               m_inflationEpochSnapshot,
               m_mintAuthorizationRecord,
               m_supplyExpansionRecord
           ) &&
           slashingEvidencePlanIsValid(
               m_block->index(),
               m_validatorRiskAssessments,
               m_validatorNetworkPolicies,
               m_protectionWorkRecords,
               m_lockedStakePositions,
               m_slashingEvidenceRecords,
               m_slashingPreparationRecords,
               m_slashingEvidenceSummary
           );
}

const core::Block& RuntimeBlockPipelineResult::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("RuntimeBlockPipelineResult has no finalized block.");
    }

    return m_block.value();
}

const consensus::QuorumCertificate& RuntimeBlockPipelineResult::certificate() const {
    return m_certificate;
}

const consensus::FinalizedBlockRecord& RuntimeBlockPipelineResult::finalizedRecord() const {
    return m_finalizedRecord;
}

const std::vector<std::string>& RuntimeBlockPipelineResult::finalizedTransactionIds() const {
    return m_finalizedTransactionIds;
}

const std::string& RuntimeBlockPipelineResult::postStateRoot() const {
    return m_postStateRoot;
}

utils::Amount RuntimeBlockPipelineResult::totalFee() const {
    return m_totalFee;
}

const std::vector<RewardDistribution>& RuntimeBlockPipelineResult::rewardDistributions() const {
    return m_rewardDistributions;
}

const std::vector<LockedStakePosition>& RuntimeBlockPipelineResult::lockedStakePositions() const {
    return m_lockedStakePositions;
}

const std::vector<SecurityScoreRecord>& RuntimeBlockPipelineResult::securityScoreRecords() const {
    return m_securityScoreRecords;
}

const std::vector<ValidatorSecurityCheckpoint>& RuntimeBlockPipelineResult::securityCheckpoints() const {
    return m_securityCheckpoints;
}

const std::vector<ValidatorRiskAssessment>& RuntimeBlockPipelineResult::validatorRiskAssessments() const {
    return m_validatorRiskAssessments;
}

const std::vector<ValidatorContainmentDecision>& RuntimeBlockPipelineResult::validatorContainmentDecisions() const {
    return m_validatorContainmentDecisions;
}

const std::vector<ValidatorNetworkPolicy>& RuntimeBlockPipelineResult::validatorNetworkPolicies() const {
    return m_validatorNetworkPolicies;
}

const MonetaryFirewallAudit& RuntimeBlockPipelineResult::monetaryFirewallAudit() const {
    return m_monetaryFirewallAudit;
}

const GenesisTreasurySnapshot& RuntimeBlockPipelineResult::genesisTreasurySnapshot() const {
    return m_genesisTreasurySnapshot;
}

const ProtectionRewardBudget& RuntimeBlockPipelineResult::protectionRewardBudget() const {
    return m_protectionRewardBudget;
}

const std::vector<ProtectionRewardGrant>& RuntimeBlockPipelineResult::protectionRewardGrants() const {
    return m_protectionRewardGrants;
}

const std::vector<ProtectionWorkRecord>& RuntimeBlockPipelineResult::protectionWorkRecords() const {
    return m_protectionWorkRecords;
}

const ProtectionRewardSummary& RuntimeBlockPipelineResult::protectionRewardSummary() const {
    return m_protectionRewardSummary;
}

const std::vector<ProtectionRewardSettlement>& RuntimeBlockPipelineResult::protectionRewardSettlements() const {
    return m_protectionRewardSettlements;
}

const InflationEpochSnapshot& RuntimeBlockPipelineResult::inflationEpochSnapshot() const {
    return m_inflationEpochSnapshot;
}

const MintAuthorizationRecord& RuntimeBlockPipelineResult::mintAuthorizationRecord() const {
    return m_mintAuthorizationRecord;
}

const SupplyExpansionRecord& RuntimeBlockPipelineResult::supplyExpansionRecord() const {
    return m_supplyExpansionRecord;
}

const FeeEconomicBalance& RuntimeBlockPipelineResult::feeEconomicBalance() const {
    return m_feeEconomicBalance;
}

const FeeBurnRecord& RuntimeBlockPipelineResult::feeBurnRecord() const {
    return m_feeBurnRecord;
}

const TreasuryFeeRecord& RuntimeBlockPipelineResult::treasuryFeeRecord() const {
    return m_treasuryFeeRecord;
}

const std::vector<SlashingEvidenceRecord>& RuntimeBlockPipelineResult::slashingEvidenceRecords() const {
    return m_slashingEvidenceRecords;
}

const std::vector<SlashingPreparationRecord>& RuntimeBlockPipelineResult::slashingPreparationRecords() const {
    return m_slashingPreparationRecords;
}

const SlashingEvidenceSummary& RuntimeBlockPipelineResult::slashingEvidenceSummary() const {
    return m_slashingEvidenceSummary;
}

std::string RuntimeBlockPipelineResult::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeBlockPipelineResult{"
        << "status=" << runtimeBlockPipelineStatusToString(m_status)
        << ";reason=" << m_reason
        << ";blockHash=" << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "NONE")
        << ";finalizedTransactionCount=" << m_finalizedTransactionIds.size()
        << ";postStateRoot=" << m_postStateRoot
        << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
        << ";rewardDistributionCount=" << m_rewardDistributions.size()
        << ";lockedStakePositionCount=" << m_lockedStakePositions.size()
        << ";securityScoreRecordCount=" << m_securityScoreRecords.size()
        << ";securityCheckpointCount=" << m_securityCheckpoints.size()
        << ";validatorRiskAssessmentCount=" << m_validatorRiskAssessments.size()
        << ";validatorContainmentDecisionCount=" << m_validatorContainmentDecisions.size()
        << ";validatorNetworkPolicyCount=" << m_validatorNetworkPolicies.size()
        << ";monetaryFirewallStatus=" << m_monetaryFirewallAudit.status()
        << ";genesisTreasuryStatus=" << m_genesisTreasurySnapshot.status()
        << ";protectionRewardBudgetStatus=" << m_protectionRewardBudget.status()
        << ";protectionRewardGrantCount=" << m_protectionRewardGrants.size()
        << ";protectionWorkRecordCount=" << m_protectionWorkRecords.size()
        << ";protectionRewardSummaryStatus=" << m_protectionRewardSummary.status()
        << ";protectionRewardSettlementCount=" << m_protectionRewardSettlements.size()
        << ";inflationEpochStatus=" << m_inflationEpochSnapshot.status()
        << ";mintAuthorizationStatus=" << m_mintAuthorizationRecord.status()
        << ";supplyExpansionStatus=" << m_supplyExpansionRecord.status()
        << ";feeEconomicBalanceStatus=" << m_feeEconomicBalance.status()
        << ";feeBurnStatus=" << m_feeBurnRecord.status()
        << ";treasuryFeeStatus=" << m_treasuryFeeRecord.status()
        << ";slashingEvidenceRecordCount=" << m_slashingEvidenceRecords.size()
        << ";slashingPreparationRecordCount=" << m_slashingPreparationRecords.size()
        << ";slashingEvidenceSummaryStatus=" << m_slashingEvidenceSummary.status()
        << "}";

    return oss.str();
}

RuntimeBlockPipelineResult RuntimeBlockPipeline::produceAndFinalizeNextBlock(
    NodeRuntime& runtime,
    const RuntimeBlockPipelineConfig& config,
    const crypto::Signer& localValidatorSigner
) {
    if (!config.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_CONFIG,
            "Runtime block pipeline config is invalid."
        );
    }

    if (!runtime.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_RUNTIME,
            "Node runtime is invalid."
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            runtime.config().genesisConfig().networkParameters().networkName()
        );

    if (!cryptoContext.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_CONFIG,
            "Protocol crypto context is invalid for network '"
            + runtime.config().genesisConfig().networkParameters().networkName()
            + "': "
            + cryptoContext.rejectionReason()
        );
    }

    const core::BlockProductionResult production =
        core::MempoolBlockProducer::produceCandidateBlock(
            runtime.blockchain(),
            runtime.mempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            core::BlockProductionConfig(
                config.maxTransactionsPerBlock(),
                config.minTransactionsPerBlock()
            ),
            config.timestamp()
        );

    if (!production.produced()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED,
            production.reason()
        );
    }

    core::BlockValidationResult transitionValidation;

    try {
        transitionValidation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                runtime.blockchain(),
                production.block(),
                previewContextForRuntime(runtime)
            );
    } catch (const std::exception& error) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
            error.what()
        );
    }

    if (!transitionValidation.accepted()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
            transitionValidation.reason()
        );
    }

    std::vector<consensus::ValidatorVoteRecord> votes;

    try {
        votes = buildValidatorVotes(
            runtime,
            production.block(),
            config.consensusRound(),
            config.timestamp() + 1,
            localValidatorSigner
        );
    } catch (const std::exception& error) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
            error.what()
        );
    }

    if (votes.empty()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS,
            "No active validators are available to vote."
        );
    }

    const consensus::QuorumCertificateBuildResult certificate =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            production.block().index(),
            production.block().hash(),
            production.block().previousHash(),
            config.consensusRound(),
            votes,
            runtime.validatorRegistry(),
            cryptoContext.policy(),
            cryptoContext.signatureProvider(),
            runtime.config().genesisConfig().networkParameters().quorumThresholdNumerator(),
            runtime.config().genesisConfig().networkParameters().quorumThresholdDenominator()
        );

    if (!certificate.certified()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED,
            certificate.reason()
        );
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

    try {
        feeEconomicBalance =
            FeeEconomics::buildFeeEconomicBalance(
                production.block().index(),
                transitionValidation.totalFee()
            );

        rewardDistributions =
            RewardDistributionCalculator::buildFromQuorumCertificate(
                feeEconomicBalance.validatorRewardAmount(),
                certificate.certificate(),
                production.block().index()
            );

        lockedStakePositions =
            LockedStakePositionBuilder::buildFromRewardDistributions(
                rewardDistributions
            );

        securityScoreRecords =
            SecurityScoreCalculator::buildFromLockedStakePositions(
                lockedStakePositions,
                production.block().index()
            );

        securityCheckpoints =
            ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
                securityScoreRecords,
                lockedStakePositions,
                production.block().index()
            );

        validatorRiskAssessments =
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
                securityCheckpoints
            );

        validatorContainmentDecisions =
            ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
                validatorRiskAssessments
            );

        validatorNetworkPolicies =
            ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
                validatorContainmentDecisions
            );

        feeBurnRecord =
            FeeEconomics::buildFeeBurnRecord(
                feeEconomicBalance,
                MonetaryFirewall::genesisSupply(runtime.config().genesisConfig())
            );

        treasuryFeeRecord =
            FeeEconomics::buildTreasuryFeeRecord(
                feeEconomicBalance
            );

        monetaryFirewallAudit =
            MonetaryFirewall::buildAudit(
                runtime.config().genesisConfig(),
                production.block().index(),
                utils::Amount(),
                feeBurnRecord.burnAmount(),
                treasuryFeeRecord.treasuryAmount(),
                utils::Amount()
            );

        genesisTreasurySnapshot =
            ProtectionTreasury::buildGenesisTreasurySnapshot(
                runtime.config().genesisConfig(),
                production.block().index(),
                treasuryFeeRecord.treasuryAmount()
            );

        protectionRewardBudget =
            ProtectionTreasury::buildProtectionRewardBudget(
                genesisTreasurySnapshot,
                rewardDistributions
            );

        protectionRewardGrants =
            ProtectionTreasury::buildProtectionRewardGrants(
                protectionRewardBudget,
                rewardDistributions,
                securityScoreRecords
            );

        protectionWorkRecords =
            ProtectionRewards::buildWorkRecords(
                protectionRewardGrants,
                securityScoreRecords,
                validatorRiskAssessments,
                validatorNetworkPolicies
            );

        protectionRewardSettlements =
            ProtectionRewards::buildSettlements(
                protectionRewardGrants,
                protectionWorkRecords
            );

        protectionRewardSummary =
            ProtectionRewards::buildSummary(
                protectionRewardBudget,
                protectionRewardSettlements
            );

        inflationEpochSnapshot =
            ControlledIssuance::buildInflationEpochSnapshot(
                runtime.config().genesisConfig(),
                production.block().index(),
                monetaryFirewallAudit.annualMintUsedAfter()
            );

        mintAuthorizationRecord =
            ControlledIssuance::buildNoMintAuthorization(
                inflationEpochSnapshot
            );

        supplyExpansionRecord =
            ControlledIssuance::buildNoSupplyExpansion(
                mintAuthorizationRecord,
                inflationEpochSnapshot
            );

        slashingEvidenceRecords =
            SlashingEvidence::buildEvidenceRecords(
                validatorRiskAssessments,
                validatorNetworkPolicies,
                protectionWorkRecords
            );

        slashingPreparationRecords =
            SlashingEvidence::buildPreparationRecords(
                slashingEvidenceRecords,
                lockedStakePositions
            );

        slashingEvidenceSummary =
            SlashingEvidence::buildSummary(
                production.block().index(),
                slashingEvidenceRecords,
                slashingPreparationRecords
            );
    } catch (const std::exception& error) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
            std::string("Economic accounting failed: ") + error.what()
        );
    }

    const consensus::BlockFinalizationResult finalization =
        consensus::BlockFinalizer::finalizeBlock(
            runtime.mutableBlockchain(),
            production.block(),
            certificate.certificate(),
            runtime.validatorRegistry(),
            runtime.mutableFinalizationRegistry(),
            cryptoContext.policy(),
            cryptoContext.signatureProvider(),
            config.timestamp() + 2
        );

    if (!finalization.finalized() &&
        !finalization.duplicate()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
            finalization.reason()
        );
    }

    const std::vector<std::string> finalizedTransactionIds =
        production.plan().transactionIds();

    removeFinalizedTransactionsFromMempool(
        runtime,
        finalizedTransactionIds
    );

    return RuntimeBlockPipelineResult::finalized(
        production.block(),
        certificate.certificate(),
        finalization.record(),
        finalizedTransactionIds,
        transitionValidation.stateRoot(),
        transitionValidation.totalFee(),
        rewardDistributions,
        lockedStakePositions,
        securityScoreRecords,
        securityCheckpoints,
        validatorRiskAssessments,
        validatorContainmentDecisions,
        validatorNetworkPolicies,
        monetaryFirewallAudit,
        genesisTreasurySnapshot,
        protectionRewardBudget,
        protectionRewardGrants,
        protectionWorkRecords,
        protectionRewardSummary,
        protectionRewardSettlements,
        inflationEpochSnapshot,
        mintAuthorizationRecord,
        supplyExpansionRecord,
        feeEconomicBalance,
        feeBurnRecord,
        treasuryFeeRecord,
        slashingEvidenceRecords,
        slashingPreparationRecords,
        slashingEvidenceSummary
    );
}

std::vector<consensus::ValidatorVoteRecord> RuntimeBlockPipeline::buildValidatorVotes(
    const NodeRuntime& runtime,
    const core::Block& block,
    std::uint64_t consensusRound,
    std::int64_t timestamp,
    const crypto::Signer& localValidatorSigner
) {
    std::vector<consensus::ValidatorVoteRecord> votes;

    votes.push_back(
        consensus::ValidatorVoteBuilder::buildApprovalVote(
            runtime.validatorRegistry(),
            block,
            consensusRound,
            timestamp,
            localValidatorSigner
        )
    );

    return votes;
}

void RuntimeBlockPipeline::removeFinalizedTransactionsFromMempool(
    NodeRuntime& runtime,
    const std::vector<std::string>& transactionIds
) {
    for (const std::string& transactionId : transactionIds) {
        runtime.mutableMempool().removeTransaction(transactionId);
    }
}

} // namespace nodo::node
