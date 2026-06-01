#include "node/RuntimeStateLoader.hpp"

#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeStateVerifier.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/StateTransitionPreview.hpp"
#include "crypto/ProtocolCryptoContext.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnits(
    const config::GenesisConfig& genesisConfig
) {
    const std::uint64_t minimumFee =
        genesisConfig.networkParameters().minimumFeeRawUnits();

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(minimumFee);
}

std::uint64_t expectedQuorumVoteCount(
    const config::GenesisConfig& genesisConfig,
    const core::ValidatorRegistry& validatorRegistry
) {
    return consensus::QuorumCertificateBuilder::requiredVoteCount(
        validatorRegistry.activeCount(),
        genesisConfig.networkParameters().quorumThresholdNumerator(),
        genesisConfig.networkParameters().quorumThresholdDenominator()
    );
}

} // namespace

std::string runtimeStateLoadStatusToString(
    RuntimeStateLoadStatus status
) {
    switch (status) {
        case RuntimeStateLoadStatus::LOADED:
            return "LOADED";
        case RuntimeStateLoadStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case RuntimeStateLoadStatus::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case RuntimeStateLoadStatus::GENESIS_MISMATCH:
            return "GENESIS_MISMATCH";
        case RuntimeStateLoadStatus::RUNTIME_START_FAILED:
            return "RUNTIME_START_FAILED";
        case RuntimeStateLoadStatus::BLOCK_FILE_MISSING:
            return "BLOCK_FILE_MISSING";
        case RuntimeStateLoadStatus::BLOCK_FILE_INVALID:
            return "BLOCK_FILE_INVALID";
        case RuntimeStateLoadStatus::BLOCK_APPEND_FAILED:
            return "BLOCK_APPEND_FAILED";
        case RuntimeStateLoadStatus::MANIFEST_MISMATCH:
            return "MANIFEST_MISMATCH";
        case RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED:
            return "MEMPOOL_LOAD_FAILED";
        default:
            return "BLOCK_FILE_INVALID";
    }
}

RuntimeStateLoadResult::RuntimeStateLoadResult()
    : m_status(RuntimeStateLoadStatus::INVALID_CONFIG),
      m_reason("Uninitialized runtime state load result."),
      m_runtime(),
      m_manifest(),
      m_loadedBlockCount(0),
      m_loadedMempoolTransactionCount(0) {}

RuntimeStateLoadResult RuntimeStateLoadResult::loaded(
    NodeRuntime runtime,
    NodeRuntimeManifest manifest,
    std::size_t loadedBlockCount,
    std::size_t loadedMempoolTransactionCount
) {
    RuntimeStateLoadResult result;
    result.m_status = RuntimeStateLoadStatus::LOADED;
    result.m_reason = "";
    result.m_runtime = std::move(runtime);
    result.m_manifest = std::move(manifest);
    result.m_loadedBlockCount = loadedBlockCount;
    result.m_loadedMempoolTransactionCount = loadedMempoolTransactionCount;
    return result;
}

RuntimeStateLoadResult RuntimeStateLoadResult::rejected(
    RuntimeStateLoadStatus status,
    std::string reason
) {
    RuntimeStateLoadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

RuntimeStateLoadStatus RuntimeStateLoadResult::status() const {
    return m_status;
}

const std::string& RuntimeStateLoadResult::reason() const {
    return m_reason;
}

bool RuntimeStateLoadResult::loaded() const {
    return m_status == RuntimeStateLoadStatus::LOADED &&
           m_runtime.isValid() &&
           m_manifest.isValid();
}

const NodeRuntime& RuntimeStateLoadResult::runtime() const {
    return m_runtime;
}

const NodeRuntimeManifest& RuntimeStateLoadResult::manifest() const {
    return m_manifest;
}

std::size_t RuntimeStateLoadResult::loadedBlockCount() const {
    return m_loadedBlockCount;
}

std::size_t RuntimeStateLoadResult::loadedMempoolTransactionCount() const {
    return m_loadedMempoolTransactionCount;
}

std::string RuntimeStateLoadResult::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeStateLoadResult{"
        << "status=" << runtimeStateLoadStatusToString(m_status)
        << ";reason=" << m_reason
        << ";loadedBlockCount=" << m_loadedBlockCount
        << ";loadedMempoolTransactionCount=" << m_loadedMempoolTransactionCount
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

RuntimeStateLoadResult RuntimeStateLoader::loadFromDataDirectory(
    const NodeDataDirectoryConfig& directoryConfig,
    const config::GenesisConfig& genesisConfig,
    const p2p::PeerInfo& localPeer
) {
    if (!directoryConfig.isValid() ||
        !genesisConfig.isValid() ||
        !localPeer.isValid()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::INVALID_CONFIG, "Runtime loader config is invalid.");
    }

    const NodeDataDirectoryReadResult manifestResult =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifestResult.loaded()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::NOT_INITIALIZED, manifestResult.reason());
    }

    const NodeRuntimeManifest manifest = manifestResult.manifest();

    if (manifest.genesisConfigId() != genesisConfig.deterministicId()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::GENESIS_MISMATCH, "Data directory genesis does not match loader genesis config.");
    }

    const NodeRuntimeConfig runtimeConfig(genesisConfig, localPeer, genesisConfig.networkParameters().maxPeerCount());
    const NodeRuntimeStartResult start = NodeRuntimeFactory::startFromGenesis(runtimeConfig);

    if (!start.started()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::RUNTIME_START_FAILED, start.reason());
    }

    NodeRuntime runtime = start.runtime();

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(manifest.networkName());

    if (!cryptoContext.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Manifest network has invalid crypto context: " + cryptoContext.rejectionReason()
        );
    }

    std::size_t loadedBlockCount = 0;

    for (std::uint64_t height = 1; height <= manifest.latestBlockHeight(); ++height) {
        const std::filesystem::path blockPath =
            FinalizedBlockStore::blockFilePath(directoryConfig, height);

        if (!std::filesystem::exists(blockPath)) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_MISSING,
                "Missing finalized block file: " + blockPath.string()
            );
        }

        FinalizedBlockArtifact artifact;

        try {
            artifact = FinalizedBlockArtifactCodec::readBlockArtifactFile(blockPath);
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                "Invalid finalized block file " + blockPath.string() + ": " + error.what()
            );
        }

        const core::Block& block = artifact.block();

        if (!runtime.mutableBlockchain().canAppendBlock(block)) {
            return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_APPEND_FAILED, "Persisted block cannot append to rebuilt runtime chain.");
        }

        try {
            const std::uint64_t requiredVoteCount = expectedQuorumVoteCount(genesisConfig, runtime.validatorRegistry());

            if (artifact.quorumCertificate().requiredVoteCount() != requiredVoteCount) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": quorum certificate threshold does not match network parameters.");
            }

            if (!artifact.quorumCertificate().verify(runtime.validatorRegistry(), cryptoContext.policy(), cryptoContext.signatureProvider())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": quorum certificate failed validator vote audit.");
            }

            if (!artifact.finalizedRecord().verify(runtime.validatorRegistry(), cryptoContext.policy(), cryptoContext.signatureProvider())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": finalized block record failed audit.");
            }

            const core::StateTransitionPreviewContext previewContext =
                RuntimeAccountStateBuilder::previewContextAtTip(genesisConfig, runtime.blockchain(), minimumFeeRawUnits(genesisConfig));

            const core::StateTransitionPreviewResult preview =
                core::StateTransitionPreview::previewBlock(block, previewContext);

            if (!preview.accepted()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted block failed state preview during reload: " + preview.reason());
            }

            if (preview.stateRoot() != artifact.postStateRoot()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted block postStateRoot does not match rebuilt account state.");
            }

            if (preview.totalFee() != artifact.totalFee()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted block totalFeeRawUnits does not match rebuilt transaction fees.");
            }

            const FeeEconomicBalance expectedFeeBalance =
                FeeEconomics::buildFeeEconomicBalance(
                    block.index(),
                    preview.totalFee()
                );

            if (!FeeEconomics::sameBalance(expectedFeeBalance, artifact.feeEconomicBalance())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted fee economic balance does not match rebuilt transaction fees.");
            }

            const std::vector<RewardDistribution> expectedRewards =
                RewardDistributionCalculator::buildFromQuorumCertificate(
                    expectedFeeBalance.validatorRewardAmount(),
                    artifact.quorumCertificate(),
                    block.index()
                );

            if (!RewardDistributionCalculator::sameDistributions(expectedRewards, artifact.rewardDistributions())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted reward distributions do not match rebuilt validator fee allocation.");
            }

            const std::vector<LockedStakePosition> expectedLockedStake =
                LockedStakePositionBuilder::buildFromRewardDistributions(expectedRewards);

            if (!LockedStakePositionBuilder::samePositions(expectedLockedStake, artifact.lockedStakePositions())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted locked stake positions do not match rebuilt reward distributions.");
            }

            const std::vector<SecurityScoreRecord> expectedScores =
                SecurityScoreCalculator::buildFromLockedStakePositions(expectedLockedStake, block.index());

            if (!SecurityScoreCalculator::sameRecords(expectedScores, artifact.securityScoreRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted security score records do not match rebuilt locked stake positions.");
            }

            const std::vector<ValidatorSecurityCheckpoint> expectedCheckpoints =
                ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(expectedScores, expectedLockedStake, block.index());

            if (!ValidatorSecurityCheckpointBuilder::sameCheckpoints(expectedCheckpoints, artifact.securityCheckpoints())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted security checkpoints do not match rebuilt security score records.");
            }

            const std::vector<ValidatorRiskAssessment> expectedRiskAssessments =
                ValidatorRiskAssessmentBuilder::buildFromCheckpoints(expectedCheckpoints);

            if (!ValidatorRiskAssessmentBuilder::sameAssessments(expectedRiskAssessments, artifact.validatorRiskAssessments())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted validator risk assessments do not match rebuilt security checkpoints.");
            }

            const std::vector<ValidatorContainmentDecision> expectedContainmentDecisions =
                ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(expectedRiskAssessments);

            if (!ValidatorContainmentDecisionBuilder::sameDecisions(expectedContainmentDecisions, artifact.validatorContainmentDecisions())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted validator containment decisions do not match rebuilt risk assessments.");
            }

            const std::vector<ValidatorNetworkPolicy> expectedNetworkPolicies =
                ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(expectedContainmentDecisions);

            if (!ValidatorNetworkPolicyBuilder::samePolicies(expectedNetworkPolicies, artifact.validatorNetworkPolicies())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted validator network policies do not match rebuilt containment decisions.");
            }

            const MonetaryFirewallAudit expectedMonetaryAudit =
                MonetaryFirewall::buildAudit(
                    genesisConfig,
                    block.index(),
                    utils::Amount(),
                    artifact.feeBurnRecord().burnAmount(),
                    artifact.treasuryFeeRecord().treasuryAmount(),
                    utils::Amount()
                );

            if (!MonetaryFirewall::sameAudit(expectedMonetaryAudit, artifact.monetaryFirewallAudit())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted monetary firewall audit does not match rebuilt monetary policy.");
            }

            const GenesisTreasurySnapshot expectedTreasurySnapshot =
                ProtectionTreasury::buildGenesisTreasurySnapshot(
                    genesisConfig,
                    block.index(),
                    artifact.treasuryFeeRecord().treasuryAmount()
                );

            if (!ProtectionTreasury::sameTreasurySnapshot(expectedTreasurySnapshot, artifact.genesisTreasurySnapshot())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted genesis treasury snapshot does not match rebuilt genesis treasury.");
            }

            const ProtectionRewardBudget expectedProtectionBudget =
                ProtectionTreasury::buildProtectionRewardBudget(
                    expectedTreasurySnapshot,
                    artifact.rewardDistributions()
                );

            if (!ProtectionTreasury::sameBudget(expectedProtectionBudget, artifact.protectionRewardBudget())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted protection reward budget does not match rebuilt treasury budget.");
            }

            const std::vector<ProtectionRewardGrant> expectedProtectionGrants =
                ProtectionTreasury::buildProtectionRewardGrants(
                    expectedProtectionBudget,
                    artifact.rewardDistributions(),
                    artifact.securityScoreRecords()
                );

            if (!ProtectionTreasury::sameGrants(expectedProtectionGrants, artifact.protectionRewardGrants())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted protection reward grants do not match rebuilt reward plan.");
            }

            const std::vector<ProtectionWorkRecord> expectedProtectionWorkRecords =
                ProtectionRewards::buildWorkRecords(
                    expectedProtectionGrants,
                    expectedScores,
                    expectedRiskAssessments,
                    expectedNetworkPolicies
                );

            if (!ProtectionRewards::sameWorkRecords(expectedProtectionWorkRecords, artifact.protectionWorkRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted protection work records do not match rebuilt security context.");
            }

            const std::vector<ProtectionRewardSettlement> expectedProtectionSettlements =
                ProtectionRewards::buildSettlements(
                    expectedProtectionGrants,
                    expectedProtectionWorkRecords
                );

            if (!ProtectionRewards::sameSettlements(expectedProtectionSettlements, artifact.protectionRewardSettlements())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted protection reward settlements do not match rebuilt work records.");
            }

            const ProtectionRewardSummary expectedProtectionSummary =
                ProtectionRewards::buildSummary(
                    expectedProtectionBudget,
                    expectedProtectionSettlements
                );

            if (!ProtectionRewards::sameSummary(expectedProtectionSummary, artifact.protectionRewardSummary())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted protection reward summary does not match rebuilt settlements.");
            }

            const InflationEpochSnapshot expectedInflationEpoch =
                ControlledIssuance::buildInflationEpochSnapshot(
                    genesisConfig,
                    block.index(),
                    artifact.monetaryFirewallAudit().annualMintUsedAfter()
                );

            if (!ControlledIssuance::sameEpoch(expectedInflationEpoch, artifact.inflationEpochSnapshot())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted inflation epoch does not match rebuilt controlled issuance policy.");
            }

            const MintAuthorizationRecord expectedMintAuthorization =
                ControlledIssuance::buildNoMintAuthorization(expectedInflationEpoch);

            if (!ControlledIssuance::sameAuthorization(expectedMintAuthorization, artifact.mintAuthorizationRecord())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted mint authorization does not match rebuilt controlled issuance policy.");
            }

            const SupplyExpansionRecord expectedSupplyExpansion =
                ControlledIssuance::buildNoSupplyExpansion(
                    expectedMintAuthorization,
                    expectedInflationEpoch
                );

            if (!ControlledIssuance::sameExpansion(expectedSupplyExpansion, artifact.supplyExpansionRecord())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted supply expansion does not match rebuilt controlled issuance policy.");
            }

            if (RewardDistributionCalculator::totalReward(artifact.rewardDistributions()) != expectedFeeBalance.validatorRewardAmount()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted rewards do not match validator fee allocation.");
            }

            const FeeBurnRecord expectedFeeBurn =
                FeeEconomics::buildFeeBurnRecord(
                    expectedFeeBalance,
                    artifact.feeBurnRecord().supplyBefore()
                );

            if (!FeeEconomics::sameBurn(expectedFeeBurn, artifact.feeBurnRecord())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted fee burn record does not match rebuilt fee split.");
            }

            const TreasuryFeeRecord expectedTreasuryFee =
                FeeEconomics::buildTreasuryFeeRecord(expectedFeeBalance);

            if (!FeeEconomics::sameTreasuryFee(expectedTreasuryFee, artifact.treasuryFeeRecord())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted treasury fee record does not match rebuilt fee split.");
            }

            if (artifact.feeBurnRecord().burnAmount() != artifact.monetaryFirewallAudit().supplyLedger().burned() ||
                artifact.feeBurnRecord().supplyBefore() != artifact.monetaryFirewallAudit().supplyLedger().supplyBefore() ||
                artifact.feeBurnRecord().supplyAfter() != artifact.monetaryFirewallAudit().supplyLedger().supplyAfter() ||
                artifact.treasuryFeeRecord().treasuryAmount() != artifact.monetaryFirewallAudit().supplyLedger().treasuryDelta()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted fee economics records do not match monetary firewall audit.");
            }

            const std::vector<SlashingEvidenceRecord> expectedSlashingEvidence =
                SlashingEvidence::buildEvidenceRecords(
                    artifact.validatorRiskAssessments(),
                    artifact.validatorNetworkPolicies(),
                    artifact.protectionWorkRecords()
                );

            if (!SlashingEvidence::sameEvidenceRecords(expectedSlashingEvidence, artifact.slashingEvidenceRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted slashing evidence records do not match rebuilt security evidence.");
            }

            const std::vector<SlashingPreparationRecord> expectedSlashingPreparation =
                SlashingEvidence::buildPreparationRecords(
                    expectedSlashingEvidence,
                    artifact.lockedStakePositions()
                );

            if (!SlashingEvidence::samePreparationRecords(expectedSlashingPreparation, artifact.slashingPreparationRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted slashing preparation records do not match rebuilt evidence.");
            }

            const SlashingEvidenceSummary expectedSlashingSummary =
                SlashingEvidence::buildSummary(
                    block.index(),
                    expectedSlashingEvidence,
                    expectedSlashingPreparation
                );

            if (!SlashingEvidence::sameSummary(expectedSlashingSummary, artifact.slashingEvidenceSummary())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted slashing evidence summary does not match rebuilt evidence.");
            }

            const std::vector<CryptographicSlashingEvidenceRecord> expectedCryptographicEvidence =
                CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
                    artifact.quorumCertificate().votes()
                );

            if (!CryptographicSlashing::sameEvidenceRecords(expectedCryptographicEvidence, artifact.cryptographicSlashingEvidenceRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted cryptographic slashing evidence does not match rebuilt vote evidence.");
            }

            const std::vector<StakePenaltyRecord> expectedStakePenalties =
                CryptographicSlashing::buildStakePenaltyRecords(
                    expectedCryptographicEvidence,
                    artifact.lockedStakePositions()
                );

            if (!CryptographicSlashing::sameStakePenaltyRecords(expectedStakePenalties, artifact.stakePenaltyRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted stake penalty records do not match rebuilt cryptographic evidence.");
            }

            const CryptographicSlashingSummary expectedCryptographicSummary =
                CryptographicSlashing::buildSummary(
                    block.index(),
                    expectedCryptographicEvidence,
                    expectedStakePenalties
                );

            if (!CryptographicSlashing::sameSummary(expectedCryptographicSummary, artifact.cryptographicSlashingSummary())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted cryptographic slashing summary does not match rebuilt evidence.");
            }

            const GovernancePolicySnapshot expectedGovernancePolicy =
                Governance::buildPolicySnapshot(block.index());

            if (!Governance::samePolicy(expectedGovernancePolicy, artifact.governancePolicySnapshot())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted governance policy does not match protocol policy.");
            }

            const std::vector<GovernanceActionGuard> expectedGovernanceGuards =
                Governance::buildActionGuards(expectedGovernancePolicy);

            if (!Governance::sameActionGuards(expectedGovernanceGuards, artifact.governanceActionGuards())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted governance guards do not match protocol policy.");
            }

            const GovernanceSummary expectedGovernanceSummary =
                Governance::buildSummary(block.index(), expectedGovernanceGuards);

            if (!Governance::sameSummary(expectedGovernanceSummary, artifact.governanceSummary())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted governance summary does not match governance guards.");
            }

            const std::vector<ValidatorLifecycleRecord> expectedLifecycleRecords =
                ValidatorLifecycle::buildLifecycleRecords(
                    block.index(),
                    artifact.rewardDistributions(),
                    artifact.lockedStakePositions(),
                    artifact.securityScoreRecords(),
                    artifact.protectionRewardSettlements(),
                    artifact.stakePenaltyRecords()
                );

            if (!ValidatorLifecycle::sameLifecycleRecords(expectedLifecycleRecords, artifact.validatorLifecycleRecords())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted validator lifecycle records do not match rebuilt accounting.");
            }

            const EpochAccountingRecord expectedEpochAccounting =
                ValidatorLifecycle::buildEpochAccountingRecord(
                    block.index(),
                    expectedLifecycleRecords
                );

            if (!ValidatorLifecycle::sameEpochAccounting(expectedEpochAccounting, artifact.epochAccountingRecord())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted epoch accounting does not match rebuilt validator lifecycle.");
            }

            const ValidatorLifecycleSummary expectedLifecycleSummary =
                ValidatorLifecycle::buildSummary(
                    block.index(),
                    expectedLifecycleRecords,
                    expectedEpochAccounting
                );

            if (!ValidatorLifecycle::sameSummary(expectedLifecycleSummary, artifact.validatorLifecycleSummary())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted validator lifecycle summary does not match rebuilt epoch accounting.");
            }

            const consensus::BlockFinalizationResult finalization =
                consensus::BlockFinalizer::finalizeBlock(
                    runtime.mutableBlockchain(),
                    block,
                    artifact.quorumCertificate(),
                    runtime.validatorRegistry(),
                    runtime.mutableFinalizationRegistry(),
                    cryptoContext.policy(),
                    cryptoContext.signatureProvider(),
                    artifact.finalizedRecord().finalizedAt()
                );

            if (!finalization.finalized() && !finalization.duplicate()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_APPEND_FAILED, "Invalid finalized block file " + blockPath.string() + ": " + finalization.reason());
            }

            if (finalization.record().serialize() != artifact.finalizedRecord().serialize()) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": stored finalized record does not match reconstructed finalization.");
            }
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": " + error.what());
        }

        ++loadedBlockCount;
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight() ||
        runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MANIFEST_MISMATCH, "Rebuilt chain latest block does not match manifest.");
    }

    const RuntimeStateVerificationResult stateRootVerification =
        RuntimeStateVerifier::verifyLatestStateRoot(
            genesisConfig,
            runtime.blockchain(),
            manifest.latestStateRoot()
        );

    if (!stateRootVerification.verified()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            stateRootVerification.reason()
        );
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            RuntimeAccountStateBuilder::accountStateViewAtTip(genesisConfig, runtime.blockchain(), minimumFeeRawUnits(genesisConfig)),
            minimumFeeRawUnits(genesisConfig),
            cryptoContext.userSignatureProvider()
        );

    if (!mempoolLoad.loaded()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED, mempoolLoad.reason());
    }

    if (!runtime.isValid()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::RUNTIME_START_FAILED, "Rebuilt runtime failed final audit.");
    }

    return RuntimeStateLoadResult::loaded(runtime, manifest, loadedBlockCount, mempoolLoad.loadedTransactionCount());
}

} // namespace nodo::node
