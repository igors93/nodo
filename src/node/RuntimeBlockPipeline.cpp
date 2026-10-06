#include "node/RuntimeBlockPipeline.hpp"

#include "node/EpochRewardSettlementService.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/FeeEconomics.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedSlashingEvidenceAudit.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/RuntimeMonetaryValidation.hpp"
#include "node/StakingRegistry.hpp"
#include "node/StateSnapshot.hpp"
#include "node/TreasuryExecutionEvidenceBuilder.hpp"
#include "node/ValidatorLifecycle.hpp"

#include "node/consensus/BlockProductionPhase.hpp"
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

} // namespace

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

  if (!runtime.validatorSetHistory().hasSet(block.index())) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
        "Historical validator set is missing for certified block.");
  }
  try {
    const auto &params = runtime.config().genesisConfig().networkParameters();
    const auto &historicalSet = runtime.validatorSetHistory().setAt(block.index());
    const std::uint64_t requiredWeight =
        consensus::QuorumCertificateBuilder::requiredVotingWeight(
            historicalSet.totalConsensusWeight(),
            params.quorumThresholdNumerator(), params.quorumThresholdDenominator());
    if (certificate.requiredVotingWeight() != requiredWeight ||
        certificate.totalVotingWeight() != historicalSet.totalConsensusWeight() ||
        certificate.validatorSetRoot() != historicalSet.validatorSetRoot()) {
      return RuntimeBlockPipelineResult::rejected(
          RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
          "Certified block quorum does not match historical network parameters.");
    }
  } catch (const std::exception &error) {
    return RuntimeBlockPipelineResult::rejected(
        RuntimeBlockPipelineStatus::FINALIZATION_FAILED, error.what());
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
                runtime, minimumFeeRawUnitsForRuntime(runtime)),
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
  std::vector<ValidatorRiskEvidenceRecord> slashingEvidenceRecords;
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
    runtime.mutableBurnRecords() = executionTracker->burns;

    const FinalizedSlashingEvidenceAuditResult slashingAudit =
        FinalizedSlashingEvidenceAudit::auditBlockEffects(
            block, runtime.validatorPenaltyLedger(),
            runtime.validatorRegistry(), runtime.stakingRegistry());
    if (!slashingAudit.passed()) {
      throw std::logic_error("Finalized slashing evidence audit failed: " +
                             slashingAudit.reason());
    }

    removeFinalizedTransactionsFromMempool(runtime, finalizedTransactionIds);
    core::AccountStateView newAccounts;
    for (const auto &account : transitionValidation.resultingAccounts()) {
      newAccounts.putAccount(account);
    }
    runtime.setCachedAccountStateAtTip(std::move(newAccounts));

    finalResult.m_receiptsRoot = transitionValidation.receiptsRoot();

    if (!finalResult.postStateRoot().empty()) {
      runtime.mutableStatePruner().recordStateRoot(block.index(),
                                                   finalResult.postStateRoot());
      runtime.mutableStatePruner().pruneHistory(block.index());
    }

    if (block.index() > 0 && block.index() % NODO_VALIDATOR_EPOCH_BLOCKS == 0) {
      const std::int64_t boundaryTimestamp = block.timestamp();
      try {
        const FastSyncSnapshot epochSnapshot =
            FastSyncSnapshot::fromRuntime(runtime, boundaryTimestamp);
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
