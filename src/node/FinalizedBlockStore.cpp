#include "node/FinalizedBlockStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <sstream>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

constexpr const char* FINALIZED_BLOCK_VERSION =
    "NODO_FINALIZED_BLOCK_V10";

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
