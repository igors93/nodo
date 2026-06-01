#include "node/RuntimeStateLoader.hpp"

#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedArtifactValidator.hpp"
#include "node/FinalityArtifactValidator.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeStateVerifier.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "crypto/ProtocolCryptoContext.hpp"

#include <exception>
#include <filesystem>
#include <limits>
#include <sstream>
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

RuntimeStateLoadStatus statusForArtifactValidation(
    const ArtifactValidationResult& result
) {
    if (result.appendFailed()) {
        return RuntimeStateLoadStatus::BLOCK_APPEND_FAILED;
    }

    return RuntimeStateLoadStatus::BLOCK_FILE_INVALID;
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

        FinalizedArtifactValidationContext validationContext(
            genesisConfig,
            runtime,
            cryptoContext,
            blockPath,
            expectedQuorumVoteCount(genesisConfig, runtime.validatorRegistry()),
            minimumFeeRawUnits(genesisConfig)
        );

        const ArtifactValidationResult validation =
            FinalizedArtifactValidator::validate(
                validationContext,
                artifact
            );

        if (!validation.accepted()) {
            return RuntimeStateLoadResult::rejected(
                statusForArtifactValidation(validation),
                validation.reason()
            );
        }

        const ArtifactValidationResult finalization =
            FinalityArtifactValidator::applyFinalization(
                validationContext,
                artifact
            );

        if (!finalization.accepted()) {
            return RuntimeStateLoadResult::rejected(
                statusForArtifactValidation(finalization),
                finalization.reason()
            );
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
