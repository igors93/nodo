#include "node/FinalizedBlockStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <sstream>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

constexpr const char* FINALIZED_BLOCK_VERSION =
    "NODO_FINALIZED_BLOCK_V18";

} // namespace

std::string finalizedBlockStoreStatusToString(
    FinalizedBlockStoreStatus status
) {
    switch (status) {
        case FinalizedBlockStoreStatus::STORED:
            return "STORED";
        case FinalizedBlockStoreStatus::ALREADY_STORED:
            return "ALREADY_STORED";
        case FinalizedBlockStoreStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case FinalizedBlockStoreStatus::INVALID_RUNTIME:
            return "INVALID_RUNTIME";
        case FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT:
            return "INVALID_PIPELINE_RESULT";
        case FinalizedBlockStoreStatus::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case FinalizedBlockStoreStatus::CONFLICTING_BLOCK_FILE:
            return "CONFLICTING_BLOCK_FILE";
        case FinalizedBlockStoreStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

FinalizedBlockStoreResult::FinalizedBlockStoreResult()
    : m_status(FinalizedBlockStoreStatus::IO_ERROR),
      m_reason("Uninitialized finalized block store result."),
      m_manifest(),
      m_blockPath() {}

FinalizedBlockStoreResult FinalizedBlockStoreResult::stored(
    NodeRuntimeManifest manifest,
    std::filesystem::path blockPath
) {
    FinalizedBlockStoreResult result;
    result.m_status = FinalizedBlockStoreStatus::STORED;
    result.m_reason = "";
    result.m_manifest = std::move(manifest);
    result.m_blockPath = std::move(blockPath);
    return result;
}

FinalizedBlockStoreResult FinalizedBlockStoreResult::alreadyStored(
    NodeRuntimeManifest manifest,
    std::filesystem::path blockPath
) {
    FinalizedBlockStoreResult result;
    result.m_status = FinalizedBlockStoreStatus::ALREADY_STORED;
    result.m_reason = "Finalized block artifact is already stored.";
    result.m_manifest = std::move(manifest);
    result.m_blockPath = std::move(blockPath);
    return result;
}

FinalizedBlockStoreResult FinalizedBlockStoreResult::rejected(
    FinalizedBlockStoreStatus status,
    std::string reason
) {
    FinalizedBlockStoreResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

FinalizedBlockStoreStatus FinalizedBlockStoreResult::status() const {
    return m_status;
}

const std::string& FinalizedBlockStoreResult::reason() const {
    return m_reason;
}

const NodeRuntimeManifest& FinalizedBlockStoreResult::manifest() const {
    return m_manifest;
}

const std::filesystem::path& FinalizedBlockStoreResult::blockPath() const {
    return m_blockPath;
}

bool FinalizedBlockStoreResult::stored() const {
    return m_status == FinalizedBlockStoreStatus::STORED &&
           m_manifest.isValid();
}

bool FinalizedBlockStoreResult::alreadyStored() const {
    return m_status == FinalizedBlockStoreStatus::ALREADY_STORED &&
           m_manifest.isValid();
}

bool FinalizedBlockStoreResult::success() const {
    return stored() || alreadyStored();
}

std::string FinalizedBlockStoreResult::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockStoreResult{"
        << "status=" << finalizedBlockStoreStatusToString(m_status)
        << ";reason=" << m_reason
        << ";blockPath=" << m_blockPath.string()
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

FinalizedBlockStoreResult FinalizedBlockStore::persist(
    const NodeDataDirectoryConfig& directoryConfig,
    const NodeRuntime& runtime,
    const RuntimeBlockPipelineResult& pipelineResult,
    std::int64_t updatedAt
) {
    if (!directoryConfig.isValid()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!runtime.isValid()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_RUNTIME,
            "Runtime is invalid."
        );
    }

    if (!pipelineResult.finalized()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result is not finalized."
        );
    }

    if (!pipelineResult.monetaryFirewallAudit().passed()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed monetary firewall audit."
        );
    }

    if (!pipelineResult.genesisTreasurySnapshot().active() ||
        !pipelineResult.protectionRewardBudget().active()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed protection treasury audit."
        );
    }

    if (!pipelineResult.inflationEpochSnapshot().active() ||
        !pipelineResult.mintAuthorizationRecord().isValid() ||
        !pipelineResult.supplyExpansionRecord().isValid()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed controlled issuance audit."
        );
    }

    if (!pipelineResult.feeEconomicBalance().active() ||
        !pipelineResult.feeBurnRecord().active() ||
        !pipelineResult.treasuryFeeRecord().active()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed fee economics audit."
        );
    }

    if (!pipelineResult.protectionRewardSummary().active() ||
        pipelineResult.protectionWorkRecords().size() != pipelineResult.protectionRewardGrants().size() ||
        pipelineResult.protectionRewardSettlements().size() != pipelineResult.protectionRewardGrants().size()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed real protection reward audit."
        );
    }

    if (!pipelineResult.slashingEvidenceSummary().active()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed slashing evidence audit."
        );
    }

    if (!pipelineResult.cryptographicSlashingSummary().active()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed cryptographic slashing audit."
        );
    }

    if (!pipelineResult.governancePolicySnapshot().active() ||
        pipelineResult.governanceActionGuards().empty() ||
        !pipelineResult.governanceSummary().active()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result failed governance audit."
        );
    }

    const NodeDataDirectoryReadResult existingManifest =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!existingManifest.loaded()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::NOT_INITIALIZED,
            existingManifest.reason()
        );
    }

    if (existingManifest.manifest().genesisConfigId() !=
        runtime.config().genesisConfig().deterministicId()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_RUNTIME,
            "Runtime genesis does not match data directory manifest."
        );
    }

    const std::filesystem::path path =
        blockFilePath(
            directoryConfig,
            pipelineResult.block().index()
        );

    const std::string contents =
        finalizedBlockFileContents(pipelineResult);

    try {
        std::filesystem::create_directories(
            directoryConfig.blocksDirectoryPath()
        );

        if (std::filesystem::exists(path)) {
            const std::string existingContents =
                readTextFile(path);

            if (existingContents != contents) {
                return FinalizedBlockStoreResult::rejected(
                    FinalizedBlockStoreStatus::CONFLICTING_BLOCK_FILE,
                    "A different finalized block artifact already exists at this height."
                );
            }

            const NodeDataDirectoryReadResult snapshot =
                NodeDataDirectory::writeRuntimeSnapshot(
                    directoryConfig,
                    runtime,
                    updatedAt
                );

            if (!snapshot.loaded()) {
                return FinalizedBlockStoreResult::rejected(
                    FinalizedBlockStoreStatus::IO_ERROR,
                    snapshot.reason()
                );
            }

            if (snapshot.manifest().latestStateRoot() != pipelineResult.postStateRoot()) {
                return FinalizedBlockStoreResult::rejected(
                    FinalizedBlockStoreStatus::IO_ERROR,
                    "Runtime snapshot latestStateRoot does not match finalized block postStateRoot."
                );
            }

            return FinalizedBlockStoreResult::alreadyStored(
                snapshot.manifest(),
                path
            );
        }

        writeTextFile(
            path,
            contents
        );

        const NodeDataDirectoryReadResult snapshot =
            NodeDataDirectory::writeRuntimeSnapshot(
                directoryConfig,
                runtime,
                updatedAt
            );

        if (!snapshot.loaded()) {
            return FinalizedBlockStoreResult::rejected(
                FinalizedBlockStoreStatus::IO_ERROR,
                snapshot.reason()
            );
        }

        if (snapshot.manifest().latestStateRoot() != pipelineResult.postStateRoot()) {
            return FinalizedBlockStoreResult::rejected(
                FinalizedBlockStoreStatus::IO_ERROR,
                "Runtime snapshot latestStateRoot does not match finalized block postStateRoot."
            );
        }

        return FinalizedBlockStoreResult::stored(
            snapshot.manifest(),
            path
        );
    } catch (const std::exception& error) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

std::filesystem::path FinalizedBlockStore::blockFilePath(
    const NodeDataDirectoryConfig& directoryConfig,
    std::uint64_t blockIndex
) {
    std::ostringstream filename;
    filename << "block_" << blockIndex << ".nodo";

    return directoryConfig.blocksDirectoryPath() / filename.str();
}

std::string FinalizedBlockStore::finalizedBlockFileContents(
    const RuntimeBlockPipelineResult& pipelineResult
) {
    std::vector<std::pair<std::string, std::string>> fields = {
        {"blockIndex", std::to_string(pipelineResult.block().index())},
        {"blockHash", pipelineResult.block().hash()},
        {"previousHash", pipelineResult.block().previousHash()},
        {"postStateRoot", pipelineResult.postStateRoot()},
        {"totalFeeRawUnits", std::to_string(pipelineResult.totalFee().rawUnits())},
        {"rewardDistributionCount", std::to_string(pipelineResult.rewardDistributions().size())},
        {"lockedStakePositionCount", std::to_string(pipelineResult.lockedStakePositions().size())},
        {"securityScoreRecordCount", std::to_string(pipelineResult.securityScoreRecords().size())},
        {"securityCheckpointCount", std::to_string(pipelineResult.securityCheckpoints().size())},
        {"validatorRiskAssessmentCount", std::to_string(pipelineResult.validatorRiskAssessments().size())},
        {"validatorContainmentDecisionCount", std::to_string(pipelineResult.validatorContainmentDecisions().size())},
        {"validatorNetworkPolicyCount", std::to_string(pipelineResult.validatorNetworkPolicies().size())},
        {"monetaryFirewallStatus", pipelineResult.monetaryFirewallAudit().status()},
        {"genesisTreasuryStatus", pipelineResult.genesisTreasurySnapshot().status()},
        {"protectionRewardBudgetStatus", pipelineResult.protectionRewardBudget().status()},
        {"protectionRewardGrantCount", std::to_string(pipelineResult.protectionRewardGrants().size())},
        {"protectionWorkRecordCount", std::to_string(pipelineResult.protectionWorkRecords().size())},
        {"protectionRewardSummaryStatus", pipelineResult.protectionRewardSummary().status()},
        {"protectionRewardSettlementCount", std::to_string(pipelineResult.protectionRewardSettlements().size())},
        {"inflationEpochStatus", pipelineResult.inflationEpochSnapshot().status()},
        {"mintAuthorizationStatus", pipelineResult.mintAuthorizationRecord().status()},
        {"supplyExpansionStatus", pipelineResult.supplyExpansionRecord().status()},
        {"feeEconomicBalanceStatus", pipelineResult.feeEconomicBalance().status()},
        {"feeBurnStatus", pipelineResult.feeBurnRecord().status()},
        {"treasuryFeeStatus", pipelineResult.treasuryFeeRecord().status()},
        {"slashingEvidenceRecordCount", std::to_string(pipelineResult.slashingEvidenceRecords().size())},
        {"slashingPreparationRecordCount", std::to_string(pipelineResult.slashingPreparationRecords().size())},
        {"slashingEvidenceSummaryStatus", pipelineResult.slashingEvidenceSummary().status()},
        {"cryptographicSlashingEvidenceCount", std::to_string(pipelineResult.cryptographicSlashingEvidenceRecords().size())},
        {"stakePenaltyRecordCount", std::to_string(pipelineResult.stakePenaltyRecords().size())},
        {"cryptographicSlashingSummaryStatus", pipelineResult.cryptographicSlashingSummary().status()},
        {"governancePolicyStatus", pipelineResult.governancePolicySnapshot().status()},
        {"governanceActionGuardCount", std::to_string(pipelineResult.governanceActionGuards().size())},
        {"governanceSummaryStatus", pipelineResult.governanceSummary().status()},
        {"timestamp", std::to_string(pipelineResult.block().timestamp())},
        {"recordCount", std::to_string(pipelineResult.block().records().size())}
    };

    const auto& records =
        pipelineResult.block().records();

    for (std::size_t index = 0; index < records.size(); ++index) {
        fields.emplace_back(
            "record." + std::to_string(index),
            records[index].serialize()
        );
    }

    const std::vector<RewardDistribution>& rewards =
        pipelineResult.rewardDistributions();

    for (std::size_t index = 0; index < rewards.size(); ++index) {
        const std::string prefix =
            "reward." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", rewards[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(rewards[index].blockHeight()));
        fields.emplace_back(prefix + "totalRewardRawUnits", std::to_string(rewards[index].totalReward().rawUnits()));
        fields.emplace_back(prefix + "liquidRewardRawUnits", std::to_string(rewards[index].liquidReward().rawUnits()));
        fields.emplace_back(prefix + "lockedRewardRawUnits", std::to_string(rewards[index].lockedReward().rawUnits()));
        fields.emplace_back(prefix + "reason", rewards[index].reason());
    }

    const std::vector<LockedStakePosition>& lockedStakePositions =
        pipelineResult.lockedStakePositions();

    for (std::size_t index = 0; index < lockedStakePositions.size(); ++index) {
        const std::string prefix =
            "lockedStake." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "ownerAddress", lockedStakePositions[index].ownerAddress());
        fields.emplace_back(prefix + "amountRawUnits", std::to_string(lockedStakePositions[index].amount().rawUnits()));
        fields.emplace_back(prefix + "createdAtHeight", std::to_string(lockedStakePositions[index].createdAtHeight()));
        fields.emplace_back(prefix + "unlockAtHeight", std::to_string(lockedStakePositions[index].unlockAtHeight()));
        fields.emplace_back(prefix + "slashable", lockedStakePositions[index].slashable() ? "true" : "false");
        fields.emplace_back(prefix + "sourceRewardId", lockedStakePositions[index].sourceRewardId());
    }

    const std::vector<SecurityScoreRecord>& securityScoreRecords =
        pipelineResult.securityScoreRecords();

    for (std::size_t index = 0; index < securityScoreRecords.size(); ++index) {
        const std::string prefix =
            "securityScore." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", securityScoreRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(securityScoreRecords[index].blockHeight()));
        fields.emplace_back(prefix + "score", std::to_string(securityScoreRecords[index].score()));
        fields.emplace_back(prefix + "lockedStakeScore", std::to_string(securityScoreRecords[index].lockedStakeScore()));
        fields.emplace_back(prefix + "participationScore", std::to_string(securityScoreRecords[index].participationScore()));
        fields.emplace_back(prefix + "maturityScore", std::to_string(securityScoreRecords[index].maturityScore()));
        fields.emplace_back(prefix + "penaltyScore", std::to_string(securityScoreRecords[index].penaltyScore()));
        fields.emplace_back(prefix + "reason", securityScoreRecords[index].reason());
        fields.emplace_back(prefix + "sourceLockedStakeId", securityScoreRecords[index].sourceLockedStakeId());
    }

    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints =
        pipelineResult.securityCheckpoints();

    for (std::size_t index = 0; index < securityCheckpoints.size(); ++index) {
        const std::string prefix =
            "securityCheckpoint." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", securityCheckpoints[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(securityCheckpoints[index].blockHeight()));
        fields.emplace_back(prefix + "score", std::to_string(securityCheckpoints[index].score()));
        fields.emplace_back(prefix + "band", securityCheckpoints[index].band());
        fields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(securityCheckpoints[index].lockedStake().rawUnits()));
        fields.emplace_back(prefix + "securityScoreRecordCount", std::to_string(securityCheckpoints[index].securityScoreRecordCount()));
        fields.emplace_back(prefix + "reason", securityCheckpoints[index].reason());
        fields.emplace_back(prefix + "sourceDigest", securityCheckpoints[index].sourceDigest());
    }

    const std::vector<ValidatorRiskAssessment>& riskAssessments =
        pipelineResult.validatorRiskAssessments();

    for (std::size_t index = 0; index < riskAssessments.size(); ++index) {
        const std::string prefix =
            "validatorRisk." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", riskAssessments[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(riskAssessments[index].blockHeight()));
        fields.emplace_back(prefix + "score", std::to_string(riskAssessments[index].score()));
        fields.emplace_back(prefix + "band", riskAssessments[index].band());
        fields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(riskAssessments[index].lockedStake().rawUnits()));
        fields.emplace_back(prefix + "riskScore", std::to_string(riskAssessments[index].riskScore()));
        fields.emplace_back(prefix + "riskLevel", riskAssessments[index].riskLevel());
        fields.emplace_back(prefix + "recommendedAction", riskAssessments[index].recommendedAction());
        fields.emplace_back(prefix + "reason", riskAssessments[index].reason());
        fields.emplace_back(prefix + "checkpointDigest", riskAssessments[index].checkpointDigest());
    }

    const std::vector<ValidatorContainmentDecision>& containmentDecisions =
        pipelineResult.validatorContainmentDecisions();

    for (std::size_t index = 0; index < containmentDecisions.size(); ++index) {
        const std::string prefix =
            "validatorContainment." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", containmentDecisions[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(containmentDecisions[index].blockHeight()));
        fields.emplace_back(prefix + "riskLevel", containmentDecisions[index].riskLevel());
        fields.emplace_back(prefix + "recommendedAction", containmentDecisions[index].recommendedAction());
        fields.emplace_back(prefix + "containmentMode", containmentDecisions[index].containmentMode());
        fields.emplace_back(prefix + "peerTrustState", containmentDecisions[index].peerTrustState());
        fields.emplace_back(prefix + "networkAdmissionState", containmentDecisions[index].networkAdmissionState());
        fields.emplace_back(prefix + "reason", containmentDecisions[index].reason());
        fields.emplace_back(prefix + "sourceRiskDigest", containmentDecisions[index].sourceRiskDigest());
    }

    const std::vector<ValidatorNetworkPolicy>& networkPolicies =
        pipelineResult.validatorNetworkPolicies();

    for (std::size_t index = 0; index < networkPolicies.size(); ++index) {
        const std::string prefix =
            "validatorNetworkPolicy." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", networkPolicies[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(networkPolicies[index].blockHeight()));
        fields.emplace_back(prefix + "containmentMode", networkPolicies[index].containmentMode());
        fields.emplace_back(prefix + "peerTrustState", networkPolicies[index].peerTrustState());
        fields.emplace_back(prefix + "networkAdmissionState", networkPolicies[index].networkAdmissionState());
        fields.emplace_back(prefix + "connectionPolicy", networkPolicies[index].connectionPolicy());
        fields.emplace_back(prefix + "messagePolicy", networkPolicies[index].messagePolicy());
        fields.emplace_back(prefix + "consensusPolicy", networkPolicies[index].consensusPolicy());
        fields.emplace_back(prefix + "requiresManualReview", networkPolicies[index].requiresManualReview() ? "true" : "false");
        fields.emplace_back(prefix + "reason", networkPolicies[index].reason());
        fields.emplace_back(prefix + "sourceContainmentDigest", networkPolicies[index].sourceContainmentDigest());
    }

    const MonetaryFirewallAudit& monetaryAudit =
        pipelineResult.monetaryFirewallAudit();

    fields.emplace_back("monetary.blockHeight", std::to_string(monetaryAudit.supplyLedger().blockHeight()));
    fields.emplace_back("monetary.supplyBeforeRawUnits", std::to_string(monetaryAudit.supplyLedger().supplyBefore().rawUnits()));
    fields.emplace_back("monetary.mintedRawUnits", std::to_string(monetaryAudit.supplyLedger().minted().rawUnits()));
    fields.emplace_back("monetary.burnedRawUnits", std::to_string(monetaryAudit.supplyLedger().burned().rawUnits()));
    fields.emplace_back("monetary.treasuryDeltaRawUnits", std::to_string(monetaryAudit.supplyLedger().treasuryDelta().rawUnits()));
    fields.emplace_back("monetary.supplyAfterRawUnits", std::to_string(monetaryAudit.supplyLedger().supplyAfter().rawUnits()));
    fields.emplace_back("monetary.annualMintLimitRawUnits", std::to_string(monetaryAudit.annualMintLimit().rawUnits()));
    fields.emplace_back("monetary.annualMintUsedBeforeRawUnits", std::to_string(monetaryAudit.annualMintUsedBefore().rawUnits()));
    fields.emplace_back("monetary.annualMintUsedAfterRawUnits", std::to_string(monetaryAudit.annualMintUsedAfter().rawUnits()));
    fields.emplace_back("monetary.policyId", monetaryAudit.policyId());
    fields.emplace_back("monetary.reason", monetaryAudit.reason());

    const GenesisTreasurySnapshot& treasurySnapshot =
        pipelineResult.genesisTreasurySnapshot();

    fields.emplace_back("treasury.treasuryAddress", treasurySnapshot.treasuryAddress());
    fields.emplace_back("treasury.blockHeight", std::to_string(treasurySnapshot.blockHeight()));
    fields.emplace_back("treasury.genesisTreasuryBalanceRawUnits", std::to_string(treasurySnapshot.genesisTreasuryBalance().rawUnits()));
    fields.emplace_back("treasury.protectedReserveRawUnits", std::to_string(treasurySnapshot.protectedReserve().rawUnits()));
    fields.emplace_back("treasury.protectionBudgetRawUnits", std::to_string(treasurySnapshot.protectionBudget().rawUnits()));
    fields.emplace_back("treasury.availableBalanceRawUnits", std::to_string(treasurySnapshot.availableBalance().rawUnits()));
    fields.emplace_back("treasury.reason", treasurySnapshot.reason());

    const ProtectionRewardBudget& protectionBudget =
        pipelineResult.protectionRewardBudget();

    fields.emplace_back("protectionBudget.blockHeight", std::to_string(protectionBudget.blockHeight()));
    fields.emplace_back("protectionBudget.treasuryAddress", protectionBudget.treasuryAddress());
    fields.emplace_back("protectionBudget.availableBudgetRawUnits", std::to_string(protectionBudget.availableBudget().rawUnits()));
    fields.emplace_back("protectionBudget.plannedTotalRawUnits", std::to_string(protectionBudget.plannedTotal().rawUnits()));
    fields.emplace_back("protectionBudget.remainingBudgetRawUnits", std::to_string(protectionBudget.remainingBudget().rawUnits()));
    fields.emplace_back("protectionBudget.beneficiaryCount", std::to_string(protectionBudget.beneficiaryCount()));
    fields.emplace_back("protectionBudget.reason", protectionBudget.reason());
    fields.emplace_back("protectionBudget.sourceTreasuryDigest", protectionBudget.sourceTreasuryDigest());

    const std::vector<ProtectionRewardGrant>& protectionGrants =
        pipelineResult.protectionRewardGrants();

    for (std::size_t index = 0; index < protectionGrants.size(); ++index) {
        const std::string prefix =
            "protectionGrant." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", protectionGrants[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(protectionGrants[index].blockHeight()));
        fields.emplace_back(prefix + "plannedRewardRawUnits", std::to_string(protectionGrants[index].plannedReward().rawUnits()));
        fields.emplace_back(prefix + "securityScore", std::to_string(protectionGrants[index].securityScore()));
        fields.emplace_back(prefix + "reason", protectionGrants[index].reason());
        fields.emplace_back(prefix + "sourceBudgetDigest", protectionGrants[index].sourceBudgetDigest());
    }

    const std::vector<ProtectionWorkRecord>& protectionWorkRecords =
        pipelineResult.protectionWorkRecords();

    for (std::size_t index = 0; index < protectionWorkRecords.size(); ++index) {
        const std::string prefix =
            "protectionWork." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", protectionWorkRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(protectionWorkRecords[index].blockHeight()));
        fields.emplace_back(prefix + "uptimeScore", std::to_string(protectionWorkRecords[index].uptimeScore()));
        fields.emplace_back(prefix + "correctVoteScore", std::to_string(protectionWorkRecords[index].correctVoteScore()));
        fields.emplace_back(prefix + "attackDetectionScore", std::to_string(protectionWorkRecords[index].attackDetectionScore()));
        fields.emplace_back(prefix + "auditContributionScore", std::to_string(protectionWorkRecords[index].auditContributionScore()));
        fields.emplace_back(prefix + "securityScore", std::to_string(protectionWorkRecords[index].securityScore()));
        fields.emplace_back(prefix + "riskPenaltyScore", std::to_string(protectionWorkRecords[index].riskPenaltyScore()));
        fields.emplace_back(prefix + "totalWorkScore", std::to_string(protectionWorkRecords[index].totalWorkScore()));
        fields.emplace_back(prefix + "reason", protectionWorkRecords[index].reason());
        fields.emplace_back(prefix + "sourceSecurityDigest", protectionWorkRecords[index].sourceSecurityDigest());
    }

    const ProtectionRewardSummary& protectionSummary =
        pipelineResult.protectionRewardSummary();

    fields.emplace_back("protectionSummary.blockHeight", std::to_string(protectionSummary.blockHeight()));
    fields.emplace_back("protectionSummary.plannedTotalRawUnits", std::to_string(protectionSummary.plannedTotal().rawUnits()));
    fields.emplace_back("protectionSummary.earnedTotalRawUnits", std::to_string(protectionSummary.earnedTotal().rawUnits()));
    fields.emplace_back("protectionSummary.deferredTotalRawUnits", std::to_string(protectionSummary.deferredTotal().rawUnits()));
    fields.emplace_back("protectionSummary.beneficiaryCount", std::to_string(protectionSummary.beneficiaryCount()));
    fields.emplace_back("protectionSummary.reason", protectionSummary.reason());
    fields.emplace_back("protectionSummary.sourceBudgetDigest", protectionSummary.sourceBudgetDigest());

    const std::vector<ProtectionRewardSettlement>& protectionSettlements =
        pipelineResult.protectionRewardSettlements();

    for (std::size_t index = 0; index < protectionSettlements.size(); ++index) {
        const std::string prefix =
            "protectionSettlement." + std::to_string(index) + ".";

        fields.emplace_back(prefix + "validatorAddress", protectionSettlements[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(protectionSettlements[index].blockHeight()));
        fields.emplace_back(prefix + "plannedRewardRawUnits", std::to_string(protectionSettlements[index].plannedReward().rawUnits()));
        fields.emplace_back(prefix + "earnedRewardRawUnits", std::to_string(protectionSettlements[index].earnedReward().rawUnits()));
        fields.emplace_back(prefix + "deferredRewardRawUnits", std::to_string(protectionSettlements[index].deferredReward().rawUnits()));
        fields.emplace_back(prefix + "workScore", std::to_string(protectionSettlements[index].workScore()));
        fields.emplace_back(prefix + "securityScore", std::to_string(protectionSettlements[index].securityScore()));
        fields.emplace_back(prefix + "reason", protectionSettlements[index].reason());
        fields.emplace_back(prefix + "sourceGrantDigest", protectionSettlements[index].sourceGrantDigest());
        fields.emplace_back(prefix + "sourceWorkDigest", protectionSettlements[index].sourceWorkDigest());
    }

    const InflationEpochSnapshot& inflationEpoch =
        pipelineResult.inflationEpochSnapshot();

    fields.emplace_back("inflationEpoch.blockHeight", std::to_string(inflationEpoch.blockHeight()));
    fields.emplace_back("inflationEpoch.epochStartBlock", std::to_string(inflationEpoch.epochStartBlock()));
    fields.emplace_back("inflationEpoch.epochEndBlock", std::to_string(inflationEpoch.epochEndBlock()));
    fields.emplace_back("inflationEpoch.maxAnnualInflationBasisPoints", std::to_string(inflationEpoch.maxAnnualInflationBasisPoints()));
    fields.emplace_back("inflationEpoch.baseSupplyRawUnits", std::to_string(inflationEpoch.baseSupply().rawUnits()));
    fields.emplace_back("inflationEpoch.annualMintLimitRawUnits", std::to_string(inflationEpoch.annualMintLimit().rawUnits()));
    fields.emplace_back("inflationEpoch.mintedThisEpochRawUnits", std::to_string(inflationEpoch.mintedThisEpoch().rawUnits()));
    fields.emplace_back("inflationEpoch.remainingMintCapacityRawUnits", std::to_string(inflationEpoch.remainingMintCapacity().rawUnits()));
    fields.emplace_back("inflationEpoch.policyId", inflationEpoch.policyId());
    fields.emplace_back("inflationEpoch.reason", inflationEpoch.reason());

    const MintAuthorizationRecord& mintAuthorization =
        pipelineResult.mintAuthorizationRecord();

    fields.emplace_back("mintAuthorization.blockHeight", std::to_string(mintAuthorization.blockHeight()));
    fields.emplace_back("mintAuthorization.authorizationId", mintAuthorization.authorizationId());
    fields.emplace_back("mintAuthorization.authorizedAmountRawUnits", std::to_string(mintAuthorization.authorizedAmount().rawUnits()));
    fields.emplace_back("mintAuthorization.activationBlock", std::to_string(mintAuthorization.activationBlock()));
    fields.emplace_back("mintAuthorization.expirationBlock", std::to_string(mintAuthorization.expirationBlock()));
    fields.emplace_back("mintAuthorization.requiredApprovalBasisPoints", std::to_string(mintAuthorization.requiredApprovalBasisPoints()));
    fields.emplace_back("mintAuthorization.timelockBlocks", std::to_string(mintAuthorization.timelockBlocks()));
    fields.emplace_back("mintAuthorization.governanceDigest", mintAuthorization.governanceDigest());
    fields.emplace_back("mintAuthorization.reason", mintAuthorization.reason());
    fields.emplace_back("mintAuthorization.sourceEpochDigest", mintAuthorization.sourceEpochDigest());

    const SupplyExpansionRecord& supplyExpansion =
        pipelineResult.supplyExpansionRecord();

    fields.emplace_back("supplyExpansion.blockHeight", std::to_string(supplyExpansion.blockHeight()));
    fields.emplace_back("supplyExpansion.mintedAmountRawUnits", std::to_string(supplyExpansion.mintedAmount().rawUnits()));
    fields.emplace_back("supplyExpansion.recipientAddress", supplyExpansion.recipientAddress());
    fields.emplace_back("supplyExpansion.authorizationId", supplyExpansion.authorizationId());
    fields.emplace_back("supplyExpansion.policyId", supplyExpansion.policyId());
    fields.emplace_back("supplyExpansion.reason", supplyExpansion.reason());
    fields.emplace_back("supplyExpansion.sourceAuthorizationDigest", supplyExpansion.sourceAuthorizationDigest());

    const FeeEconomicBalance& feeBalance =
        pipelineResult.feeEconomicBalance();

    fields.emplace_back("feeBalance.blockHeight", std::to_string(feeBalance.blockHeight()));
    fields.emplace_back("feeBalance.totalFeeRawUnits", std::to_string(feeBalance.totalFee().rawUnits()));
    fields.emplace_back("feeBalance.validatorRewardRawUnits", std::to_string(feeBalance.validatorRewardAmount().rawUnits()));
    fields.emplace_back("feeBalance.treasuryRawUnits", std::to_string(feeBalance.treasuryAmount().rawUnits()));
    fields.emplace_back("feeBalance.burnRawUnits", std::to_string(feeBalance.burnAmount().rawUnits()));
    fields.emplace_back("feeBalance.policyId", feeBalance.policyId());
    fields.emplace_back("feeBalance.reason", feeBalance.reason());

    const FeeBurnRecord& feeBurn =
        pipelineResult.feeBurnRecord();

    fields.emplace_back("feeBurn.blockHeight", std::to_string(feeBurn.blockHeight()));
    fields.emplace_back("feeBurn.burnAmountRawUnits", std::to_string(feeBurn.burnAmount().rawUnits()));
    fields.emplace_back("feeBurn.supplyBeforeRawUnits", std::to_string(feeBurn.supplyBefore().rawUnits()));
    fields.emplace_back("feeBurn.supplyAfterRawUnits", std::to_string(feeBurn.supplyAfter().rawUnits()));
    fields.emplace_back("feeBurn.reason", feeBurn.reason());
    fields.emplace_back("feeBurn.sourceFeeBalanceDigest", feeBurn.sourceFeeBalanceDigest());

    const TreasuryFeeRecord& treasuryFee =
        pipelineResult.treasuryFeeRecord();

    fields.emplace_back("treasuryFee.blockHeight", std::to_string(treasuryFee.blockHeight()));
    fields.emplace_back("treasuryFee.treasuryAddress", treasuryFee.treasuryAddress());
    fields.emplace_back("treasuryFee.treasuryAmountRawUnits", std::to_string(treasuryFee.treasuryAmount().rawUnits()));
    fields.emplace_back("treasuryFee.reason", treasuryFee.reason());
    fields.emplace_back("treasuryFee.sourceFeeBalanceDigest", treasuryFee.sourceFeeBalanceDigest());

    const std::vector<SlashingEvidenceRecord>& evidenceRecords =
        pipelineResult.slashingEvidenceRecords();

    for (std::size_t index = 0; index < evidenceRecords.size(); ++index) {
        const std::string prefix = "slashingEvidence." + std::to_string(index) + ".";
        fields.emplace_back(prefix + "validatorAddress", evidenceRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(evidenceRecords[index].blockHeight()));
        fields.emplace_back(prefix + "evidenceType", evidenceRecords[index].evidenceType());
        fields.emplace_back(prefix + "severityScore", std::to_string(evidenceRecords[index].severityScore()));
        fields.emplace_back(prefix + "slashable", evidenceRecords[index].slashable() ? "true" : "false");
        fields.emplace_back(prefix + "recommendedAction", evidenceRecords[index].recommendedAction());
        fields.emplace_back(prefix + "reason", evidenceRecords[index].reason());
        fields.emplace_back(prefix + "sourceSecurityDigest", evidenceRecords[index].sourceSecurityDigest());
    }

    const std::vector<SlashingPreparationRecord>& preparationRecords =
        pipelineResult.slashingPreparationRecords();

    for (std::size_t index = 0; index < preparationRecords.size(); ++index) {
        const std::string prefix = "slashingPreparation." + std::to_string(index) + ".";
        fields.emplace_back(prefix + "validatorAddress", preparationRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(preparationRecords[index].blockHeight()));
        fields.emplace_back(prefix + "evidenceCount", std::to_string(preparationRecords[index].evidenceCount()));
        fields.emplace_back(prefix + "slashableEvidenceCount", std::to_string(preparationRecords[index].slashableEvidenceCount()));
        fields.emplace_back(prefix + "maxSeverityScore", std::to_string(preparationRecords[index].maxSeverityScore()));
        fields.emplace_back(prefix + "preparedPenaltyRawUnits", std::to_string(preparationRecords[index].preparedPenaltyAmount().rawUnits()));
        fields.emplace_back(prefix + "enforcementAction", preparationRecords[index].enforcementAction());
        fields.emplace_back(prefix + "reason", preparationRecords[index].reason());
        fields.emplace_back(prefix + "sourceEvidenceDigest", preparationRecords[index].sourceEvidenceDigest());
    }

    const SlashingEvidenceSummary& evidenceSummary =
        pipelineResult.slashingEvidenceSummary();

    fields.emplace_back("slashingSummary.blockHeight", std::to_string(evidenceSummary.blockHeight()));
    fields.emplace_back("slashingSummary.evidenceCount", std::to_string(evidenceSummary.evidenceCount()));
    fields.emplace_back("slashingSummary.slashableEvidenceCount", std::to_string(evidenceSummary.slashableEvidenceCount()));
    fields.emplace_back("slashingSummary.maxSeverityScore", std::to_string(evidenceSummary.maxSeverityScore()));
    fields.emplace_back("slashingSummary.preparedPenaltyTotalRawUnits", std::to_string(evidenceSummary.preparedPenaltyTotal().rawUnits()));
    fields.emplace_back("slashingSummary.reason", evidenceSummary.reason());
    fields.emplace_back("slashingSummary.sourcePreparationDigest", evidenceSummary.sourcePreparationDigest());

    const std::vector<CryptographicSlashingEvidenceRecord>& cryptoEvidenceRecords =
        pipelineResult.cryptographicSlashingEvidenceRecords();

    for (std::size_t index = 0; index < cryptoEvidenceRecords.size(); ++index) {
        const std::string prefix = "cryptographicSlashingEvidence." + std::to_string(index) + ".";
        fields.emplace_back(prefix + "validatorAddress", cryptoEvidenceRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(cryptoEvidenceRecords[index].blockHeight()));
        fields.emplace_back(prefix + "round", std::to_string(cryptoEvidenceRecords[index].round()));
        fields.emplace_back(prefix + "evidenceType", cryptoEvidenceRecords[index].evidenceType());
        fields.emplace_back(prefix + "severityScore", std::to_string(cryptoEvidenceRecords[index].severityScore()));
        fields.emplace_back(prefix + "penaltyBasisPoints", std::to_string(cryptoEvidenceRecords[index].penaltyBasisPoints()));
        fields.emplace_back(prefix + "firstVoteDigest", cryptoEvidenceRecords[index].firstVoteDigest());
        fields.emplace_back(prefix + "secondVoteDigest", cryptoEvidenceRecords[index].secondVoteDigest());
        fields.emplace_back(prefix + "reason", cryptoEvidenceRecords[index].reason());
        fields.emplace_back(prefix + "sourceEvidenceDigest", cryptoEvidenceRecords[index].sourceEvidenceDigest());
    }

    const std::vector<StakePenaltyRecord>& stakePenaltyRecords =
        pipelineResult.stakePenaltyRecords();

    for (std::size_t index = 0; index < stakePenaltyRecords.size(); ++index) {
        const std::string prefix = "stakePenalty." + std::to_string(index) + ".";
        fields.emplace_back(prefix + "validatorAddress", stakePenaltyRecords[index].validatorAddress());
        fields.emplace_back(prefix + "blockHeight", std::to_string(stakePenaltyRecords[index].blockHeight()));
        fields.emplace_back(prefix + "lockedStakeBeforeRawUnits", std::to_string(stakePenaltyRecords[index].lockedStakeBefore().rawUnits()));
        fields.emplace_back(prefix + "penaltyAmountRawUnits", std::to_string(stakePenaltyRecords[index].penaltyAmount().rawUnits()));
        fields.emplace_back(prefix + "lockedStakeAfterRawUnits", std::to_string(stakePenaltyRecords[index].lockedStakeAfter().rawUnits()));
        fields.emplace_back(prefix + "evidenceCount", std::to_string(stakePenaltyRecords[index].evidenceCount()));
        fields.emplace_back(prefix + "reason", stakePenaltyRecords[index].reason());
        fields.emplace_back(prefix + "sourceEvidenceDigest", stakePenaltyRecords[index].sourceEvidenceDigest());
    }

    const CryptographicSlashingSummary& cryptoSlashingSummary =
        pipelineResult.cryptographicSlashingSummary();

    fields.emplace_back("cryptographicSlashingSummary.blockHeight", std::to_string(cryptoSlashingSummary.blockHeight()));
    fields.emplace_back("cryptographicSlashingSummary.evidenceCount", std::to_string(cryptoSlashingSummary.evidenceCount()));
    fields.emplace_back("cryptographicSlashingSummary.slashableEvidenceCount", std::to_string(cryptoSlashingSummary.slashableEvidenceCount()));
    fields.emplace_back("cryptographicSlashingSummary.maxSeverityScore", std::to_string(cryptoSlashingSummary.maxSeverityScore()));
    fields.emplace_back("cryptographicSlashingSummary.penaltyTotalRawUnits", std::to_string(cryptoSlashingSummary.penaltyTotal().rawUnits()));
    fields.emplace_back("cryptographicSlashingSummary.reason", cryptoSlashingSummary.reason());
    fields.emplace_back("cryptographicSlashingSummary.sourcePenaltyDigest", cryptoSlashingSummary.sourcePenaltyDigest());

    const GovernancePolicySnapshot& governancePolicy =
        pipelineResult.governancePolicySnapshot();

    fields.emplace_back("governancePolicy.blockHeight", std::to_string(governancePolicy.blockHeight()));
    fields.emplace_back("governancePolicy.requiredApprovalBasisPoints", std::to_string(governancePolicy.requiredApprovalBasisPoints()));
    fields.emplace_back("governancePolicy.timelockBlocks", std::to_string(governancePolicy.timelockBlocks()));
    fields.emplace_back("governancePolicy.activationDelayBlocks", std::to_string(governancePolicy.activationDelayBlocks()));
    fields.emplace_back("governancePolicy.policyId", governancePolicy.policyId());
    fields.emplace_back("governancePolicy.reason", governancePolicy.reason());

    const std::vector<GovernanceActionGuard>& governanceGuards =
        pipelineResult.governanceActionGuards();

    for (std::size_t index = 0; index < governanceGuards.size(); ++index) {
        const std::string prefix = "governanceGuard." + std::to_string(index) + ".";
        fields.emplace_back(prefix + "actionType", governanceGuards[index].actionType());
        fields.emplace_back(prefix + "status", governanceGuards[index].status());
        fields.emplace_back(prefix + "blockHeight", std::to_string(governanceGuards[index].blockHeight()));
        fields.emplace_back(prefix + "protectedResource", governanceGuards[index].protectedResource());
        fields.emplace_back(prefix + "requiredApprovalBasisPoints", std::to_string(governanceGuards[index].requiredApprovalBasisPoints()));
        fields.emplace_back(prefix + "timelockBlocks", std::to_string(governanceGuards[index].timelockBlocks()));
        fields.emplace_back(prefix + "reason", governanceGuards[index].reason());
        fields.emplace_back(prefix + "sourcePolicyDigest", governanceGuards[index].sourcePolicyDigest());
    }

    const GovernanceSummary& governanceSummary =
        pipelineResult.governanceSummary();

    fields.emplace_back("governanceSummary.blockHeight", std::to_string(governanceSummary.blockHeight()));
    fields.emplace_back("governanceSummary.guardCount", std::to_string(governanceSummary.guardCount()));
    fields.emplace_back("governanceSummary.activeProposalCount", std::to_string(governanceSummary.activeProposalCount()));
    fields.emplace_back("governanceSummary.approvedProposalCount", std::to_string(governanceSummary.approvedProposalCount()));
    fields.emplace_back("governanceSummary.executableProposalCount", std::to_string(governanceSummary.executableProposalCount()));
    fields.emplace_back("governanceSummary.executedProposalCount", std::to_string(governanceSummary.executedProposalCount()));
    fields.emplace_back("governanceSummary.reason", governanceSummary.reason());
    fields.emplace_back("governanceSummary.sourceGuardDigest", governanceSummary.sourceGuardDigest());

    fields.emplace_back(
        "block",
        pipelineResult.block().serialize()
    );

    fields.emplace_back(
        "quorumCertificate",
        pipelineResult.certificate().serialize()
    );

    fields.emplace_back(
        "finalizedRecord",
        pipelineResult.finalizedRecord().serialize()
    );

    return serialization::KeyValueFileCodec::serialize(
        FINALIZED_BLOCK_VERSION,
        fields
    );
}

void FinalizedBlockStore::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    storage::AtomicFile::writeTextFile(
        path,
        contents
    );
}

std::string FinalizedBlockStore::readTextFile(
    const std::filesystem::path& path
) {
    return storage::AtomicFile::readTextFile(path);
}

} // namespace nodo::node
