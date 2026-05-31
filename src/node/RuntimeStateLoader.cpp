#include "node/RuntimeStateLoader.hpp"

#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "core/StateTransitionPreview.hpp"
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
    "NODO_FINALIZED_BLOCK_V2";

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
      m_postStateRoot("") {}

FinalizedBlockArtifact::FinalizedBlockArtifact(
    core::Block block,
    std::string postStateRoot
)
    : m_block(std::move(block)),
      m_postStateRoot(std::move(postStateRoot)) {}

const core::Block& FinalizedBlockArtifact::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("FinalizedBlockArtifact has no block.");
    }

    return m_block.value();
}

const std::string& FinalizedBlockArtifact::postStateRoot() const {
    return m_postStateRoot;
}

bool FinalizedBlockArtifact::isValid() const {
    return m_block.has_value() &&
           m_block->isValid() &&
           !m_postStateRoot.empty();
}

std::string FinalizedBlockArtifact::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockArtifact{"
        << "blockHash=" << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "INVALID")
        << ";postStateRoot=" << m_postStateRoot
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
            std::stoull(document.requireField("recordCount"))
        );

    std::set<std::string> allowedFields = {
        "blockIndex",
        "blockHash",
        "previousHash",
        "postStateRoot",
        "timestamp",
        "recordCount",
        "block",
        "quorumCertificate",
        "finalizedRecord"
    };

    for (std::size_t index = 0; index < recordCount; ++index) {
        allowedFields.insert("record." + std::to_string(index));
    }

    document.requireOnlyFields(allowedFields);

    /*
     * V2 stores explicit block fields and record lines, but the canonical block
     * serialization is still kept as an integrity anchor.
     */
    const std::string serializedBlock =
        document.requireField("block");

    core::Block block =
        serialization::BlockCodec::deserialize(serializedBlock);

    const std::uint64_t index =
        static_cast<std::uint64_t>(
            std::stoull(document.requireField("blockIndex"))
        );

    const std::string blockHash =
        document.requireField("blockHash");

    const std::string previousHash =
        document.requireField("previousHash");

    const std::string postStateRoot =
        document.requireField("postStateRoot");

    const std::int64_t timestamp =
        std::stoll(document.requireField("timestamp"));

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

    return FinalizedBlockArtifact(
        block,
        postStateRoot
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
                error.what()
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
                    "Persisted block failed state preview during reload: "
                    + preview.reason()
                );
            }

            if (preview.stateRoot() != artifact.postStateRoot()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Persisted block postStateRoot does not match rebuilt account state."
                );
            }
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                error.what()
            );
        }

        runtime.mutableBlockchain().addBlock(block);
        ++loadedBlockCount;
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight() ||
        runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Rebuilt chain latest block does not match manifest."
        );
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION
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
