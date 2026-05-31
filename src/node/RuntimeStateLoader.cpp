#include "node/RuntimeStateLoader.hpp"

#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/StateTransitionPreview.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr const char* FINALIZED_BLOCK_VERSION =
    "NODO_FINALIZED_BLOCK_V8";

std::string readTextFile(
    const std::filesystem::path& path
) {
    return storage::AtomicFile::readTextFile(path);
}

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

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::uint64_t parsed =
        static_cast<std::uint64_t>(
            std::stoull(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

std::uint16_t parseU16Strict(
    const std::string& value,
    const std::string& fieldName
) {
    const std::uint64_t parsed =
        parseU64Strict(
            value,
            fieldName
        );

    if (parsed > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Numeric field exceeds uint16 range: " + fieldName);
    }

    return static_cast<std::uint16_t>(parsed);
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }

        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::int64_t parsed =
        static_cast<std::int64_t>(
            std::stoll(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

utils::Amount parseAmountStrict(
    const std::string& value,
    const std::string& fieldName
) {
    return utils::Amount::fromRawUnits(
        parseI64Strict(
            value,
            fieldName
        )
    );
}

bool parseBoolStrict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value == "true") {
        return true;
    }

    if (value == "false") {
        return false;
    }

    throw std::invalid_argument("Malformed boolean field: " + fieldName);
}

RewardDistribution parseRewardDistribution(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "reward." + std::to_string(index) + ".";

    RewardDistribution distribution(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseAmountStrict(
            document.requireField(prefix + "totalRewardRawUnits"),
            prefix + "totalRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "liquidRewardRawUnits"),
            prefix + "liquidRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "lockedRewardRawUnits"),
            prefix + "lockedRewardRawUnits"
        ),
        document.requireField(prefix + "reason")
    );

    if (!distribution.isValid()) {
        throw std::invalid_argument("Finalized block reward distribution is invalid.");
    }

    return distribution;
}

LockedStakePosition parseLockedStakePosition(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "lockedStake." + std::to_string(index) + ".";

    LockedStakePosition position(
        document.requireField(prefix + "ownerAddress"),
        parseAmountStrict(
            document.requireField(prefix + "amountRawUnits"),
            prefix + "amountRawUnits"
        ),
        parseU64Strict(
            document.requireField(prefix + "createdAtHeight"),
            prefix + "createdAtHeight"
        ),
        parseU64Strict(
            document.requireField(prefix + "unlockAtHeight"),
            prefix + "unlockAtHeight"
        ),
        parseBoolStrict(
            document.requireField(prefix + "slashable"),
            prefix + "slashable"
        ),
        document.requireField(prefix + "sourceRewardId")
    );

    if (!position.isValid()) {
        throw std::invalid_argument("Finalized block locked stake position is invalid.");
    }

    return position;
}

SecurityScoreRecord parseSecurityScoreRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "securityScore." + std::to_string(index) + ".";

    SecurityScoreRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        parseU16Strict(
            document.requireField(prefix + "lockedStakeScore"),
            prefix + "lockedStakeScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "participationScore"),
            prefix + "participationScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "maturityScore"),
            prefix + "maturityScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "penaltyScore"),
            prefix + "penaltyScore"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceLockedStakeId")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block security score record is invalid.");
    }

    return record;
}
ValidatorSecurityCheckpoint parseSecurityCheckpoint(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "securityCheckpoint." + std::to_string(index) + ".";

    ValidatorSecurityCheckpoint checkpoint(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        document.requireField(prefix + "band"),
        parseAmountStrict(
            document.requireField(prefix + "lockedStakeRawUnits"),
            prefix + "lockedStakeRawUnits"
        ),
        parseU64Strict(
            document.requireField(prefix + "securityScoreRecordCount"),
            prefix + "securityScoreRecordCount"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceDigest")
    );

    if (!checkpoint.isValid()) {
        throw std::invalid_argument("Finalized block security checkpoint is invalid.");
    }

    return checkpoint;
}
ValidatorRiskAssessment parseValidatorRiskAssessment(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "validatorRisk." + std::to_string(index) + ".";

    ValidatorRiskAssessment assessment(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        document.requireField(prefix + "band"),
        parseAmountStrict(
            document.requireField(prefix + "lockedStakeRawUnits"),
            prefix + "lockedStakeRawUnits"
        ),
        parseU16Strict(
            document.requireField(prefix + "riskScore"),
            prefix + "riskScore"
        ),
        document.requireField(prefix + "riskLevel"),
        document.requireField(prefix + "recommendedAction"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "checkpointDigest")
    );

    if (!assessment.isValid()) {
        throw std::invalid_argument("Finalized block validator risk assessment is invalid.");
    }

    return assessment;
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

FinalizedBlockArtifact::FinalizedBlockArtifact()
    : m_block(std::nullopt),
      m_postStateRoot(""),
      m_totalFee(),
      m_rewardDistributions(),
      m_lockedStakePositions(),
      m_securityScoreRecords(),
      m_securityCheckpoints(),
      m_validatorRiskAssessments(),
      m_quorumCertificate(),
      m_finalizedRecord() {}

FinalizedBlockArtifact::FinalizedBlockArtifact(
    core::Block block,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    consensus::QuorumCertificate quorumCertificate,
    consensus::FinalizedBlockRecord finalizedRecord
)
    : m_block(std::move(block)),
      m_postStateRoot(std::move(postStateRoot)),
      m_totalFee(totalFee),
      m_rewardDistributions(std::move(rewardDistributions)),
      m_lockedStakePositions(std::move(lockedStakePositions)),
      m_securityScoreRecords(std::move(securityScoreRecords)),
      m_securityCheckpoints(std::move(securityCheckpoints)),
      m_validatorRiskAssessments(std::move(validatorRiskAssessments)),
      m_quorumCertificate(std::move(quorumCertificate)),
      m_finalizedRecord(std::move(finalizedRecord)) {}

const core::Block& FinalizedBlockArtifact::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("FinalizedBlockArtifact has no block.");
    }

    return m_block.value();
}

const std::string& FinalizedBlockArtifact::postStateRoot() const {
    return m_postStateRoot;
}

utils::Amount FinalizedBlockArtifact::totalFee() const {
    return m_totalFee;
}

const std::vector<RewardDistribution>& FinalizedBlockArtifact::rewardDistributions() const {
    return m_rewardDistributions;
}

const std::vector<LockedStakePosition>& FinalizedBlockArtifact::lockedStakePositions() const {
    return m_lockedStakePositions;
}

const std::vector<SecurityScoreRecord>& FinalizedBlockArtifact::securityScoreRecords() const {
    return m_securityScoreRecords;
}

const std::vector<ValidatorSecurityCheckpoint>& FinalizedBlockArtifact::securityCheckpoints() const {
    return m_securityCheckpoints;
}

const std::vector<ValidatorRiskAssessment>& FinalizedBlockArtifact::validatorRiskAssessments() const {
    return m_validatorRiskAssessments;
}

const consensus::QuorumCertificate& FinalizedBlockArtifact::quorumCertificate() const {
    return m_quorumCertificate;
}

const consensus::FinalizedBlockRecord& FinalizedBlockArtifact::finalizedRecord() const {
    return m_finalizedRecord;
}

bool FinalizedBlockArtifact::isValid() const {
    if (!m_block.has_value() ||
        !m_block->isValid() ||
        m_postStateRoot.empty() ||
        m_totalFee.isNegative() ||
        !m_quorumCertificate.isStructurallyValid() ||
        !m_finalizedRecord.isStructurallyValid()) {
        return false;
    }

    try {
        if (m_totalFee.isZero()) {
            return m_rewardDistributions.empty() &&
                   m_lockedStakePositions.empty() &&
                   m_securityScoreRecords.empty() &&
                   m_securityCheckpoints.empty() &&
                   m_validatorRiskAssessments.empty();
        }

        return RewardDistributionCalculator::totalReward(m_rewardDistributions) == m_totalFee &&
               LockedStakePositionBuilder::samePositions(
                   LockedStakePositionBuilder::buildFromRewardDistributions(m_rewardDistributions),
                   m_lockedStakePositions
               ) &&
               SecurityScoreCalculator::sameRecords(
                   SecurityScoreCalculator::buildFromLockedStakePositions(m_lockedStakePositions, m_block->index()),
                   m_securityScoreRecords
               ) &&
               ValidatorSecurityCheckpointBuilder::sameCheckpoints(
                   ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(m_securityScoreRecords, m_lockedStakePositions, m_block->index()),
                   m_securityCheckpoints
               ) &&
               ValidatorRiskAssessmentBuilder::sameAssessments(
                   ValidatorRiskAssessmentBuilder::buildFromCheckpoints(m_securityCheckpoints),
                   m_validatorRiskAssessments
               );
    } catch (const std::exception&) {
        return false;
    }
}

std::string FinalizedBlockArtifact::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockArtifact{"
        << "blockHash=" << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "INVALID")
        << ";postStateRoot=" << m_postStateRoot
        << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
        << ";rewardDistributionCount=" << m_rewardDistributions.size()
        << ";lockedStakePositionCount=" << m_lockedStakePositions.size()
        << ";securityScoreRecordCount=" << m_securityScoreRecords.size()
        << ";securityCheckpointCount=" << m_securityCheckpoints.size()
        << ";validatorRiskAssessmentCount=" << m_validatorRiskAssessments.size()
        << "}";

    return oss.str();
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

core::Block FinalizedBlockFileCodec::readBlockFile(
    const std::filesystem::path& path
) {
    return readBlockArtifactFile(path).block();
}

core::Block FinalizedBlockFileCodec::decodeBlockFileContents(
    const std::string& contents
) {
    return decodeBlockArtifactFileContents(contents).block();
}

FinalizedBlockArtifact FinalizedBlockFileCodec::readBlockArtifactFile(
    const std::filesystem::path& path
) {
    return decodeBlockArtifactFileContents(
        readTextFile(path)
    );
}

FinalizedBlockArtifact FinalizedBlockFileCodec::decodeBlockArtifactFileContents(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            FINALIZED_BLOCK_VERSION
        );

    const std::size_t recordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("recordCount"), "recordCount"));
    const std::size_t rewardDistributionCount = static_cast<std::size_t>(parseU64Strict(document.requireField("rewardDistributionCount"), "rewardDistributionCount"));
    const std::size_t lockedStakePositionCount = static_cast<std::size_t>(parseU64Strict(document.requireField("lockedStakePositionCount"), "lockedStakePositionCount"));
    const std::size_t securityScoreRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("securityScoreRecordCount"), "securityScoreRecordCount"));
    const std::size_t securityCheckpointCount = static_cast<std::size_t>(parseU64Strict(document.requireField("securityCheckpointCount"), "securityCheckpointCount"));
    const std::size_t validatorRiskAssessmentCount = static_cast<std::size_t>(parseU64Strict(document.requireField("validatorRiskAssessmentCount"), "validatorRiskAssessmentCount"));

    std::set<std::string> allowedFields = {
        "blockIndex",
        "blockHash",
        "previousHash",
        "postStateRoot",
        "totalFeeRawUnits",
        "rewardDistributionCount",
        "lockedStakePositionCount",
        "securityScoreRecordCount",
        "securityCheckpointCount",
        "validatorRiskAssessmentCount",
        "timestamp",
        "recordCount",
        "block",
        "quorumCertificate",
        "finalizedRecord"
    };

    for (std::size_t index = 0; index < recordCount; ++index) {
        allowedFields.insert("record." + std::to_string(index));
    }

    for (std::size_t index = 0; index < rewardDistributionCount; ++index) {
        const std::string prefix = "reward." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "totalRewardRawUnits");
        allowedFields.insert(prefix + "liquidRewardRawUnits");
        allowedFields.insert(prefix + "lockedRewardRawUnits");
        allowedFields.insert(prefix + "reason");
    }

    for (std::size_t index = 0; index < lockedStakePositionCount; ++index) {
        const std::string prefix = "lockedStake." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "ownerAddress");
        allowedFields.insert(prefix + "amountRawUnits");
        allowedFields.insert(prefix + "createdAtHeight");
        allowedFields.insert(prefix + "unlockAtHeight");
        allowedFields.insert(prefix + "slashable");
        allowedFields.insert(prefix + "sourceRewardId");
    }

    for (std::size_t index = 0; index < securityScoreRecordCount; ++index) {
        const std::string prefix = "securityScore." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "lockedStakeScore");
        allowedFields.insert(prefix + "participationScore");
        allowedFields.insert(prefix + "maturityScore");
        allowedFields.insert(prefix + "penaltyScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceLockedStakeId");
    }

    for (std::size_t index = 0; index < securityCheckpointCount; ++index) {
        const std::string prefix = "securityCheckpoint." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "band");
        allowedFields.insert(prefix + "lockedStakeRawUnits");
        allowedFields.insert(prefix + "securityScoreRecordCount");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceDigest");
    }

    for (std::size_t index = 0; index < validatorRiskAssessmentCount; ++index) {
        const std::string prefix = "validatorRisk." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "band");
        allowedFields.insert(prefix + "lockedStakeRawUnits");
        allowedFields.insert(prefix + "riskScore");
        allowedFields.insert(prefix + "riskLevel");
        allowedFields.insert(prefix + "recommendedAction");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "checkpointDigest");
    }

    document.requireOnlyFields(allowedFields);

    /*
     * V8 stores explicit block fields, fee accounting, locked stake,
     * security score records, checkpoints and validator risk assessments. The canonical block serialization remains the
     * integrity anchor for the block payload itself.
     */
    const std::string serializedBlock = document.requireField("block");
    core::Block block = serialization::BlockCodec::deserialize(serializedBlock);

    const std::uint64_t index = static_cast<std::uint64_t>(parseU64Strict(document.requireField("blockIndex"), "blockIndex"));
    const std::string blockHash = document.requireField("blockHash");
    const std::string previousHash = document.requireField("previousHash");
    const std::string postStateRoot = document.requireField("postStateRoot");
    const utils::Amount totalFee = parseAmountStrict(document.requireField("totalFeeRawUnits"), "totalFeeRawUnits");

    std::vector<RewardDistribution> rewardDistributions;
    rewardDistributions.reserve(rewardDistributionCount);
    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributionCount; ++rewardIndex) {
        rewardDistributions.push_back(parseRewardDistribution(document, rewardIndex));
    }

    std::vector<LockedStakePosition> lockedStakePositions;
    lockedStakePositions.reserve(lockedStakePositionCount);
    for (std::size_t positionIndex = 0; positionIndex < lockedStakePositionCount; ++positionIndex) {
        lockedStakePositions.push_back(parseLockedStakePosition(document, positionIndex));
    }

    std::vector<SecurityScoreRecord> securityScoreRecords;
    securityScoreRecords.reserve(securityScoreRecordCount);
    for (std::size_t scoreIndex = 0; scoreIndex < securityScoreRecordCount; ++scoreIndex) {
        securityScoreRecords.push_back(parseSecurityScoreRecord(document, scoreIndex));
    }

    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints;
    securityCheckpoints.reserve(securityCheckpointCount);
    for (std::size_t checkpointIndex = 0; checkpointIndex < securityCheckpointCount; ++checkpointIndex) {
        securityCheckpoints.push_back(parseSecurityCheckpoint(document, checkpointIndex));
    }

    std::vector<ValidatorRiskAssessment> validatorRiskAssessments;
    validatorRiskAssessments.reserve(validatorRiskAssessmentCount);
    for (std::size_t assessmentIndex = 0; assessmentIndex < validatorRiskAssessmentCount; ++assessmentIndex) {
        validatorRiskAssessments.push_back(parseValidatorRiskAssessment(document, assessmentIndex));
    }

    const std::int64_t timestamp = parseI64Strict(document.requireField("timestamp"), "timestamp");

    const consensus::QuorumCertificate quorumCertificate =
        consensus::QuorumCertificate::deserialize(document.requireField("quorumCertificate"));

    const consensus::FinalizedBlockRecord finalizedRecord =
        consensus::FinalizedBlockRecord::deserialize(document.requireField("finalizedRecord"));

    if (postStateRoot.empty()) {
        throw std::invalid_argument("Finalized block file postStateRoot is empty.");
    }

    if (block.index() != index ||
        block.hash() != blockHash ||
        block.previousHash() != previousHash ||
        block.timestamp() != timestamp ||
        block.records().size() != recordCount) {
        throw std::invalid_argument("Finalized block file header does not match block payload.");
    }

    for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::string key = "record." + std::to_string(recordIndex);
        if (block.records()[recordIndex].serialize() != document.requireField(key)) {
            throw std::invalid_argument("Finalized block record line does not match block payload.");
        }
    }

    if (!consensus::BlockFinalizer::certificateMatchesBlock(block, quorumCertificate)) {
        throw std::invalid_argument("Finalized block quorum certificate does not match block payload.");
    }

    if (!finalizedRecord.matchesBlock(block) ||
        finalizedRecord.quorumCertificate().serialize() != quorumCertificate.serialize()) {
        throw std::invalid_argument("Finalized block record does not match block or quorum certificate.");
    }

    if (RewardDistributionCalculator::totalReward(rewardDistributions) != totalFee) {
        throw std::invalid_argument("Finalized block reward distributions do not match total fees.");
    }

    if (!LockedStakePositionBuilder::samePositions(
            LockedStakePositionBuilder::buildFromRewardDistributions(rewardDistributions),
            lockedStakePositions)) {
        throw std::invalid_argument("Finalized block locked stake positions do not match reward distributions.");
    }

    if (!SecurityScoreCalculator::sameRecords(
            SecurityScoreCalculator::buildFromLockedStakePositions(lockedStakePositions, block.index()),
            securityScoreRecords)) {
        throw std::invalid_argument("Finalized block security score records do not match locked stake positions.");
    }

    if (!ValidatorSecurityCheckpointBuilder::sameCheckpoints(
            ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(securityScoreRecords, lockedStakePositions, block.index()),
            securityCheckpoints)) {
        throw std::invalid_argument("Finalized block security checkpoints do not match security score records.");
    }

    if (!ValidatorRiskAssessmentBuilder::sameAssessments(
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(securityCheckpoints),
            validatorRiskAssessments)) {
        throw std::invalid_argument("Finalized block validator risk assessments do not match security checkpoints.");
    }

    std::vector<std::pair<std::string, std::string>> canonicalFields = {
        {"blockIndex", document.requireField("blockIndex")},
        {"blockHash", document.requireField("blockHash")},
        {"previousHash", document.requireField("previousHash")},
        {"postStateRoot", postStateRoot},
        {"totalFeeRawUnits", std::to_string(totalFee.rawUnits())},
        {"rewardDistributionCount", std::to_string(rewardDistributions.size())},
        {"lockedStakePositionCount", std::to_string(lockedStakePositions.size())},
        {"securityScoreRecordCount", std::to_string(securityScoreRecords.size())},
        {"securityCheckpointCount", std::to_string(securityCheckpoints.size())},
        {"validatorRiskAssessmentCount", std::to_string(validatorRiskAssessments.size())},
        {"timestamp", document.requireField("timestamp")},
        {"recordCount", document.requireField("recordCount")}
    };

    for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::string key = "record." + std::to_string(recordIndex);
        canonicalFields.emplace_back(key, document.requireField(key));
    }

    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributions.size(); ++rewardIndex) {
        const std::string prefix = "reward." + std::to_string(rewardIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", rewardDistributions[rewardIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(rewardDistributions[rewardIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "totalRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].totalReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "liquidRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].liquidReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "lockedRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].lockedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "reason", rewardDistributions[rewardIndex].reason());
    }

    for (std::size_t positionIndex = 0; positionIndex < lockedStakePositions.size(); ++positionIndex) {
        const std::string prefix = "lockedStake." + std::to_string(positionIndex) + ".";
        canonicalFields.emplace_back(prefix + "ownerAddress", lockedStakePositions[positionIndex].ownerAddress());
        canonicalFields.emplace_back(prefix + "amountRawUnits", std::to_string(lockedStakePositions[positionIndex].amount().rawUnits()));
        canonicalFields.emplace_back(prefix + "createdAtHeight", std::to_string(lockedStakePositions[positionIndex].createdAtHeight()));
        canonicalFields.emplace_back(prefix + "unlockAtHeight", std::to_string(lockedStakePositions[positionIndex].unlockAtHeight()));
        canonicalFields.emplace_back(prefix + "slashable", lockedStakePositions[positionIndex].slashable() ? "true" : "false");
        canonicalFields.emplace_back(prefix + "sourceRewardId", lockedStakePositions[positionIndex].sourceRewardId());
    }

    for (std::size_t scoreIndex = 0; scoreIndex < securityScoreRecords.size(); ++scoreIndex) {
        const std::string prefix = "securityScore." + std::to_string(scoreIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", securityScoreRecords[scoreIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(securityScoreRecords[scoreIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(securityScoreRecords[scoreIndex].score()));
        canonicalFields.emplace_back(prefix + "lockedStakeScore", std::to_string(securityScoreRecords[scoreIndex].lockedStakeScore()));
        canonicalFields.emplace_back(prefix + "participationScore", std::to_string(securityScoreRecords[scoreIndex].participationScore()));
        canonicalFields.emplace_back(prefix + "maturityScore", std::to_string(securityScoreRecords[scoreIndex].maturityScore()));
        canonicalFields.emplace_back(prefix + "penaltyScore", std::to_string(securityScoreRecords[scoreIndex].penaltyScore()));
        canonicalFields.emplace_back(prefix + "reason", securityScoreRecords[scoreIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceLockedStakeId", securityScoreRecords[scoreIndex].sourceLockedStakeId());
    }

    for (std::size_t checkpointIndex = 0; checkpointIndex < securityCheckpoints.size(); ++checkpointIndex) {
        const std::string prefix = "securityCheckpoint." + std::to_string(checkpointIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", securityCheckpoints[checkpointIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(securityCheckpoints[checkpointIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(securityCheckpoints[checkpointIndex].score()));
        canonicalFields.emplace_back(prefix + "band", securityCheckpoints[checkpointIndex].band());
        canonicalFields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(securityCheckpoints[checkpointIndex].lockedStake().rawUnits()));
        canonicalFields.emplace_back(prefix + "securityScoreRecordCount", std::to_string(securityCheckpoints[checkpointIndex].securityScoreRecordCount()));
        canonicalFields.emplace_back(prefix + "reason", securityCheckpoints[checkpointIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceDigest", securityCheckpoints[checkpointIndex].sourceDigest());
    }

    for (std::size_t assessmentIndex = 0; assessmentIndex < validatorRiskAssessments.size(); ++assessmentIndex) {
        const std::string prefix = "validatorRisk." + std::to_string(assessmentIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", validatorRiskAssessments[assessmentIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(validatorRiskAssessments[assessmentIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(validatorRiskAssessments[assessmentIndex].score()));
        canonicalFields.emplace_back(prefix + "band", validatorRiskAssessments[assessmentIndex].band());
        canonicalFields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(validatorRiskAssessments[assessmentIndex].lockedStake().rawUnits()));
        canonicalFields.emplace_back(prefix + "riskScore", std::to_string(validatorRiskAssessments[assessmentIndex].riskScore()));
        canonicalFields.emplace_back(prefix + "riskLevel", validatorRiskAssessments[assessmentIndex].riskLevel());
        canonicalFields.emplace_back(prefix + "recommendedAction", validatorRiskAssessments[assessmentIndex].recommendedAction());
        canonicalFields.emplace_back(prefix + "reason", validatorRiskAssessments[assessmentIndex].reason());
        canonicalFields.emplace_back(prefix + "checkpointDigest", validatorRiskAssessments[assessmentIndex].checkpointDigest());
    }

    canonicalFields.emplace_back("block", serializedBlock);
    canonicalFields.emplace_back("quorumCertificate", quorumCertificate.serialize());
    canonicalFields.emplace_back("finalizedRecord", finalizedRecord.serialize());

    const std::string canonicalContents =
        serialization::KeyValueFileCodec::serialize(FINALIZED_BLOCK_VERSION, canonicalFields);

    if (contents != canonicalContents) {
        throw std::invalid_argument("Finalized block file is not canonical.");
    }

    return FinalizedBlockArtifact(
        block,
        postStateRoot,
        totalFee,
        rewardDistributions,
        lockedStakePositions,
        securityScoreRecords,
        securityCheckpoints,
        validatorRiskAssessments,
        quorumCertificate,
        finalizedRecord
    );
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
            artifact = FinalizedBlockFileCodec::readBlockArtifactFile(blockPath);
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

            const std::vector<RewardDistribution> expectedRewards =
                RewardDistributionCalculator::buildFromQuorumCertificate(preview.totalFee(), artifact.quorumCertificate(), block.index());

            if (!RewardDistributionCalculator::sameDistributions(expectedRewards, artifact.rewardDistributions())) {
                return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::BLOCK_FILE_INVALID, "Invalid finalized block file " + blockPath.string() + ": Persisted reward distributions do not match rebuilt validator fee rewards.");
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

    try {
        const core::AccountStateView accountState =
            RuntimeAccountStateBuilder::accountStateViewAtTip(genesisConfig, runtime.blockchain(), minimumFeeRawUnits(genesisConfig));

        const std::string latestStateRoot =
            core::StateRootCalculator::calculateAccountStateRoot(accountState);

        if (latestStateRoot != manifest.latestStateRoot()) {
            return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MANIFEST_MISMATCH, "Manifest latestStateRoot does not match rebuilt account state.");
        }
    } catch (const std::exception& error) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MANIFEST_MISMATCH, error.what());
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            RuntimeAccountStateBuilder::accountStateViewAtTip(genesisConfig, runtime.blockchain(), minimumFeeRawUnits(genesisConfig)),
            minimumFeeRawUnits(genesisConfig),
            cryptoContext.signatureProvider()
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
