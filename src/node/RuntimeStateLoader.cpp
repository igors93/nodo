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
    "NODO_FINALIZED_BLOCK_V4";

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
      m_quorumCertificate(),
      m_finalizedRecord() {}

FinalizedBlockArtifact::FinalizedBlockArtifact(
    core::Block block,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    consensus::QuorumCertificate quorumCertificate,
    consensus::FinalizedBlockRecord finalizedRecord
)
    : m_block(std::move(block)),
      m_postStateRoot(std::move(postStateRoot)),
      m_totalFee(totalFee),
      m_rewardDistributions(std::move(rewardDistributions)),
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
            return m_rewardDistributions.empty();
        }

        return RewardDistributionCalculator::totalReward(
            m_rewardDistributions
        ) == m_totalFee;
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

    const std::size_t recordCount =
        static_cast<std::size_t>(
            parseU64Strict(
                document.requireField("recordCount"),
                "recordCount"
            )
        );

    const std::size_t rewardDistributionCount =
        static_cast<std::size_t>(
            parseU64Strict(
                document.requireField("rewardDistributionCount"),
                "rewardDistributionCount"
            )
        );

    std::set<std::string> allowedFields = {
        "blockIndex",
        "blockHash",
        "previousHash",
        "postStateRoot",
        "totalFeeRawUnits",
        "rewardDistributionCount",
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
        const std::string prefix =
            "reward." + std::to_string(index) + ".";

        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "totalRewardRawUnits");
        allowedFields.insert(prefix + "liquidRewardRawUnits");
        allowedFields.insert(prefix + "lockedRewardRawUnits");
        allowedFields.insert(prefix + "reason");
    }

    document.requireOnlyFields(allowedFields);

    /*
     * V4 stores explicit block fields, fee accounting and reward distribution
     * records, while the canonical block serialization remains the integrity
     * anchor.
     */
    const std::string serializedBlock =
        document.requireField("block");

    core::Block block =
        serialization::BlockCodec::deserialize(serializedBlock);

    const std::uint64_t index =
        static_cast<std::uint64_t>(
            parseU64Strict(
                document.requireField("blockIndex"),
                "blockIndex"
            )
        );

    const std::string blockHash =
        document.requireField("blockHash");

    const std::string previousHash =
        document.requireField("previousHash");

    const std::string postStateRoot =
        document.requireField("postStateRoot");

    const utils::Amount totalFee =
        parseAmountStrict(
            document.requireField("totalFeeRawUnits"),
            "totalFeeRawUnits"
        );

    std::vector<RewardDistribution> rewardDistributions;
    rewardDistributions.reserve(rewardDistributionCount);

    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributionCount; ++rewardIndex) {
        rewardDistributions.push_back(
            parseRewardDistribution(
                document,
                rewardIndex
            )
        );
    }

    const std::int64_t timestamp =
        parseI64Strict(
            document.requireField("timestamp"),
            "timestamp"
        );

    const consensus::QuorumCertificate quorumCertificate =
        consensus::QuorumCertificate::deserialize(
            document.requireField("quorumCertificate")
        );

    const consensus::FinalizedBlockRecord finalizedRecord =
        consensus::FinalizedBlockRecord::deserialize(
            document.requireField("finalizedRecord")
        );

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
        const std::string key =
            "record." + std::to_string(recordIndex);

        if (block.records()[recordIndex].serialize() != document.requireField(key)) {
            throw std::invalid_argument("Finalized block record line does not match block payload.");
        }
    }

    if (!consensus::BlockFinalizer::certificateMatchesBlock(
            block,
            quorumCertificate
        )) {
        throw std::invalid_argument("Finalized block quorum certificate does not match block payload.");
    }

    if (!finalizedRecord.matchesBlock(block) ||
        finalizedRecord.quorumCertificate().serialize() != quorumCertificate.serialize()) {
        throw std::invalid_argument("Finalized block record does not match block or quorum certificate.");
    }

    if (RewardDistributionCalculator::totalReward(rewardDistributions) != totalFee) {
        throw std::invalid_argument("Finalized block reward distributions do not match total fees.");
    }

    std::vector<std::pair<std::string, std::string>> canonicalFields = {
        {"blockIndex", document.requireField("blockIndex")},
        {"blockHash", document.requireField("blockHash")},
        {"previousHash", document.requireField("previousHash")},
        {"postStateRoot", postStateRoot},
        {"totalFeeRawUnits", std::to_string(totalFee.rawUnits())},
        {"rewardDistributionCount", std::to_string(rewardDistributions.size())},
        {"timestamp", document.requireField("timestamp")},
        {"recordCount", document.requireField("recordCount")}
    };

    for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::string key =
            "record." + std::to_string(recordIndex);

        canonicalFields.emplace_back(
            key,
            document.requireField(key)
        );
    }

    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributions.size(); ++rewardIndex) {
        const std::string prefix =
            "reward." + std::to_string(rewardIndex) + ".";

        canonicalFields.emplace_back(prefix + "validatorAddress", rewardDistributions[rewardIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(rewardDistributions[rewardIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "totalRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].totalReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "liquidRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].liquidReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "lockedRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].lockedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "reason", rewardDistributions[rewardIndex].reason());
    }

    canonicalFields.emplace_back(
        "block",
        serializedBlock
    );

    canonicalFields.emplace_back(
        "quorumCertificate",
        quorumCertificate.serialize()
    );

    canonicalFields.emplace_back(
        "finalizedRecord",
        finalizedRecord.serialize()
    );

    const std::string canonicalContents =
        serialization::KeyValueFileCodec::serialize(
            FINALIZED_BLOCK_VERSION,
            canonicalFields
        );

    if (contents != canonicalContents) {
        throw std::invalid_argument("Finalized block file is not canonical.");
    }

    return FinalizedBlockArtifact(
        block,
        postStateRoot,
        totalFee,
        rewardDistributions,
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
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::INVALID_CONFIG,
            "Runtime loader config is invalid."
        );
    }

    const NodeDataDirectoryReadResult manifestResult =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifestResult.loaded()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::NOT_INITIALIZED,
            manifestResult.reason()
        );
    }

    const NodeRuntimeManifest manifest =
        manifestResult.manifest();

    if (manifest.genesisConfigId() != genesisConfig.deterministicId()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::GENESIS_MISMATCH,
            "Data directory genesis does not match loader genesis config."
        );
    }

    const NodeRuntimeConfig runtimeConfig(
        genesisConfig,
        localPeer,
        genesisConfig.networkParameters().maxPeerCount()
    );

    const NodeRuntimeStartResult start =
        NodeRuntimeFactory::startFromGenesis(runtimeConfig);

    if (!start.started()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::RUNTIME_START_FAILED,
            start.reason()
        );
    }

    NodeRuntime runtime =
        start.runtime();

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            manifest.networkName()
        );

    if (!cryptoContext.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Manifest network has invalid crypto context: "
            + cryptoContext.rejectionReason()
        );
    }

    std::size_t loadedBlockCount = 0;

    for (std::uint64_t height = 1; height <= manifest.latestBlockHeight(); ++height) {
        const std::filesystem::path blockPath =
            FinalizedBlockStore::blockFilePath(
                directoryConfig,
                height
            );

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
                "Invalid finalized block file "
                + blockPath.string()
                + ": "
                + error.what()
            );
        }

        const core::Block& block =
            artifact.block();

        if (!runtime.mutableBlockchain().canAppendBlock(block)) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_APPEND_FAILED,
                "Persisted block cannot append to rebuilt runtime chain."
            );
        }

        try {
            const std::uint64_t requiredVoteCount =
                expectedQuorumVoteCount(
                    genesisConfig,
                    runtime.validatorRegistry()
                );

            if (artifact.quorumCertificate().requiredVoteCount() != requiredVoteCount) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": quorum certificate threshold does not match network parameters."
                );
            }

            if (!artifact.quorumCertificate().verify(
                    runtime.validatorRegistry(),
                    cryptoContext.policy(),
                    cryptoContext.signatureProvider()
                )) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": quorum certificate failed validator vote audit."
                );
            }

            if (!artifact.finalizedRecord().verify(
                    runtime.validatorRegistry(),
                    cryptoContext.policy(),
                    cryptoContext.signatureProvider()
                )) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": finalized block record failed audit."
                );
            }

            const core::StateTransitionPreviewContext previewContext =
                RuntimeAccountStateBuilder::previewContextAtTip(
                    genesisConfig,
                    runtime.blockchain(),
                    minimumFeeRawUnits(genesisConfig)
                );

            const core::StateTransitionPreviewResult preview =
                core::StateTransitionPreview::previewBlock(
                    block,
                    previewContext
                );

            if (!preview.accepted()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": "
                    "Persisted block failed state preview during reload: "
                    + preview.reason()
                );
            }

            if (preview.stateRoot() != artifact.postStateRoot()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": "
                    "Persisted block postStateRoot does not match rebuilt account state."
                );
            }

            if (preview.totalFee() != artifact.totalFee()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": "
                    "Persisted block totalFeeRawUnits does not match rebuilt transaction fees."
                );
            }

            const std::vector<RewardDistribution> expectedRewards =
                RewardDistributionCalculator::buildFromQuorumCertificate(
                    preview.totalFee(),
                    artifact.quorumCertificate(),
                    block.index()
                );

            if (!RewardDistributionCalculator::sameDistributions(
                    expectedRewards,
                    artifact.rewardDistributions()
                )) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": "
                    "Persisted reward distributions do not match rebuilt validator fee rewards."
                );
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

            if (!finalization.finalized() &&
                !finalization.duplicate()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_APPEND_FAILED,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": "
                    + finalization.reason()
                );
            }

            if (finalization.record().serialize() !=
                artifact.finalizedRecord().serialize()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Invalid finalized block file "
                    + blockPath.string()
                    + ": stored finalized record does not match reconstructed finalization."
                );
            }
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                "Invalid finalized block file "
                + blockPath.string()
                + ": "
                + error.what()
            );
        }

        ++loadedBlockCount;
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight() ||
        runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Rebuilt chain latest block does not match manifest."
        );
    }

    try {
        const core::AccountStateView accountState =
            RuntimeAccountStateBuilder::accountStateViewAtTip(
                genesisConfig,
                runtime.blockchain(),
                minimumFeeRawUnits(genesisConfig)
            );

        const std::string latestStateRoot =
            core::StateRootCalculator::calculateAccountStateRoot(
                accountState
            );

        if (latestStateRoot != manifest.latestStateRoot()) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::MANIFEST_MISMATCH,
                "Manifest latestStateRoot does not match rebuilt account state."
            );
        }
    } catch (const std::exception& error) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            error.what()
        );
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            RuntimeAccountStateBuilder::accountStateViewAtTip(
                genesisConfig,
                runtime.blockchain(),
                minimumFeeRawUnits(genesisConfig)
            ),
            minimumFeeRawUnits(genesisConfig),
            cryptoContext.signatureProvider()
        );

    if (!mempoolLoad.loaded()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED,
            mempoolLoad.reason()
        );
    }

    if (!runtime.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::RUNTIME_START_FAILED,
            "Rebuilt runtime failed final audit."
        );
    }

    return RuntimeStateLoadResult::loaded(
        runtime,
        manifest,
        loadedBlockCount,
        mempoolLoad.loadedTransactionCount()
    );
}

} // namespace nodo::node
