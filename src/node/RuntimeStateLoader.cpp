#include "node/RuntimeStateLoader.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "core/StateRootCalculator.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedArtifactValidator.hpp"
#include "node/FinalityArtifactValidator.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/ProtocolInvariantChecker.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeStateVerifier.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "storage/AccountStateSnapshotStore.hpp"

#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
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
    std::size_t loadedMempoolTransactionCount,
    std::vector<FinalizedBlockArtifact> loadedArtifacts
) {
    RuntimeStateLoadResult result;
    result.m_status = RuntimeStateLoadStatus::LOADED;
    result.m_reason = "";
    result.m_runtime = std::move(runtime);
    result.m_manifest = std::move(manifest);
    result.m_loadedBlockCount = loadedBlockCount;
    result.m_loadedMempoolTransactionCount = loadedMempoolTransactionCount;
    result.m_loadedArtifacts = std::move(loadedArtifacts);
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

const std::vector<FinalizedBlockArtifact>& RuntimeStateLoadResult::loadedArtifacts() const {
    return m_loadedArtifacts;
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

    try {
        FinalizedBlockStore::recoverInterruptedCommit(directoryConfig);
    } catch (const std::exception& error) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            std::string("Unable to recover interrupted finalization commit: ") + error.what()
        );
    }

    const NodeDataDirectoryReadResult manifestResult =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifestResult.loaded()) {
        const RuntimeStateLoadStatus status =
            manifestResult.status() == NodeDataDirectoryReadStatus::INVALID_MANIFEST
                ? RuntimeStateLoadStatus::MANIFEST_MISMATCH
                : RuntimeStateLoadStatus::NOT_INITIALIZED;

        return RuntimeStateLoadResult::rejected(status, manifestResult.reason());
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

    // Load the persisted consensus round but defer its application until after
    // block replay. Applying it here would advance the consensus height before
    // the validator-set history has entries for that height, causing
    // NodeRuntime::isValid() to return false during artifact validation.
    const std::optional<consensus::ConsensusRoundState> recoveredRound =
        consensus::ConsensusRecoveryStore::load(
            directoryConfig.consensusRecoveryPath()
        );

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(manifest.networkName());

    if (!cryptoContext.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Manifest network has invalid crypto context: " + cryptoContext.rejectionReason()
        );
    }

    std::size_t loadedBlockCount = 0;
    std::vector<FinalizedBlockArtifact> loadedArtifacts;

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

        // Enforce artifact digest integrity during reload.
        // An empty digest means the artifact cannot produce a deterministic
        // fingerprint — a sign of storage corruption or incomplete serialization.
        // A non-deterministic digest (two calls yield different results) means
        // the artifact's canonical form is unstable, which breaks availability
        // challenge binding and chain audit assignments.
        if (artifact.isValid()) {
            const std::string firstDigest = artifact.artifactDigest();
            if (firstDigest.empty()) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Failed to reload Nodo runtime: corrupted artifact at block " +
                    std::to_string(height) +
                    " produced an empty digest. Storage integrity cannot be verified."
                );
            }
            if (artifact.artifactDigest() != firstDigest) {
                return RuntimeStateLoadResult::rejected(
                    RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                    "Failed to reload Nodo runtime: non-deterministic artifact digest at block " +
                    std::to_string(height) +
                    ". Storage integrity cannot be guaranteed."
                );
            }
        }

        try {
            runtime.mutableSupplyState().applyFinalizedDelta(
                artifact.supplyDelta()
            );
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                "Supply continuity break during reload at block " +
                std::to_string(artifact.block().index()) + ": " +
                error.what()
            );
        }

        try {
            runtime.applyGovernanceFromBlock(
                artifact.block(), artifact.block().timestamp()
            );
            runtime.applySlashingEvidenceFromBlock(artifact.block());
            ProtocolStateTransition::applyValidatorEpochTransition(
                runtime.mutableValidatorRegistry(),
                artifact.block().index(),
                artifact.block().timestamp()
            );
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                "Protocol state replay failed at block " +
                    std::to_string(artifact.block().index()) + ": " +
                    error.what()
            );
        }
        if (!runtime.mutableValidatorSetHistory().recordSet(
                artifact.block().index() + 1, runtime.validatorRegistry()
            )) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                "Validator set history conflict while replaying block "
                    + std::to_string(artifact.block().index())
            );
        }

        if (!artifact.postStateRoot().empty()) {
            runtime.mutableStatePruner().recordStateRoot(
                artifact.block().index(),
                artifact.postStateRoot()
            );
        }

        loadedArtifacts.push_back(artifact);
        ++loadedBlockCount;
    }

    if (manifest.latestBlockHeight() > 0) {
        runtime.mutableStatePruner().pruneHistory(manifest.latestBlockHeight());
    }

    const std::uint64_t nextConsensusHeight =
        runtime.blockchain().latestBlock().index() + 1;
    if (runtime.consensusRoundManager().currentState().height() != nextConsensusHeight) {
        constexpr std::uint64_t nextRound = 1;
        const std::string proposer = consensus::ProposerSchedule::selectProposer(
            runtime.validatorRegistry(),
            genesisConfig.networkParameters().chainId(),
            nextConsensusHeight,
            nextRound
        );
        runtime.mutableConsensusRoundManager().advanceToHeight(
            nextConsensusHeight,
            nextRound,
            proposer,
            runtime.blockchain().latestBlock().timestamp() + 1,
            genesisConfig.networkParameters().targetBlockTimeSeconds()
        );
    }

    // Apply the deferred consensus recovery now that the validator-set history
    // is fully populated. Only restore the round if it targets the current
    // working height; a lower height means those blocks are already finalized.
    if (recoveredRound.has_value() &&
        recoveredRound->height() == nextConsensusHeight) {
        runtime.mutableConsensusRoundManager().advanceToHeight(
            recoveredRound->height(),
            recoveredRound->round(),
            recoveredRound->proposerAddress(),
            recoveredRound->roundStartedAt(),
            genesisConfig.networkParameters().targetBlockTimeSeconds()
        );
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight() ||
        runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MANIFEST_MISMATCH, "Rebuilt chain latest block does not match manifest.");
    }

    // Build the account state at the chain tip. When a valid snapshot is
    // available and its block hash matches the on-disk chain, we replay only
    // the delta (blocks after the snapshot) instead of the full O(N) replay.
    const std::int64_t minFee = minimumFeeRawUnits(genesisConfig);
    core::AccountStateView tipAccountState;
    bool usedAccountStateSnapshot = false;
    {
        const storage::AccountStateSnapshotStore snapshotStore(
            directoryConfig.rootPath()
        );
        const auto snapshotOpt = snapshotStore.load();

        if (snapshotOpt.has_value()) {
            const auto& snap = snapshotOpt.value();
            const auto& blocks = runtime.blockchain().blocks();
            if (snap.genesisConfigId() == genesisConfig.deterministicId() &&
                snap.height() < blocks.size() &&
                blocks[static_cast<std::size_t>(snap.height())].hash() == snap.blockHash()) {
                try {
                    tipAccountState = RuntimeAccountStateBuilder::accountStateViewFromSnapshot(
                        genesisConfig,
                        snap.view(),
                        runtime.blockchain(),
                        snap.height(),
                        minFee
                    );
                    usedAccountStateSnapshot = true;
                } catch (...) {}
            }
        }

        if (!usedAccountStateSnapshot) {
            tipAccountState = RuntimeAccountStateBuilder::accountStateViewAtTip(
                genesisConfig, runtime.blockchain(), minFee
            );
        }
    }

    const core::StateTransitionPreviewContext tipContext =
        RuntimeAccountStateBuilder::previewContextAtTip(runtime, minFee);
    std::string computedStateRoot =
        core::StateRootCalculator::calculateProtocolStateRoot(
            tipAccountState, tipContext.deterministicStateDomains()
        );

    if ((computedStateRoot.empty() ||
         computedStateRoot != manifest.latestStateRoot()) &&
        usedAccountStateSnapshot) {
        tipAccountState = RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig, runtime.blockchain(), minFee
        );
        computedStateRoot =
            core::StateRootCalculator::calculateProtocolStateRoot(
                tipAccountState, tipContext.deterministicStateDomains()
            );
    }

    if (computedStateRoot.empty() || computedStateRoot != manifest.latestStateRoot()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Manifest latestStateRoot does not match rebuilt protocol state."
        );
    }

    const std::uint64_t effectiveMinimumFeeRaw =
        runtime.effectiveMinimumFeeRawUnits();
    if (effectiveMinimumFeeRaw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED,
            "Governed minimum fee exceeds supported Amount range."
        );
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            tipAccountState,
            static_cast<std::int64_t>(effectiveMinimumFeeRaw),
            cryptoContext.userSignatureProvider()
        );

    if (!mempoolLoad.loaded()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED, mempoolLoad.reason());
    }

    const ProtocolInvariantCheckResult invariantCheck =
        ProtocolInvariantChecker::checkRuntimeAgainstManifest(
            runtime,
            manifest
        );

    if (!invariantCheck.passed()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::RUNTIME_START_FAILED,
            "Rebuilt runtime failed protocol invariant audit: " +
                invariantCheck.reason()
        );
    }

    if (!runtime.isValid()) {
        return RuntimeStateLoadResult::rejected(RuntimeStateLoadStatus::RUNTIME_START_FAILED, "Rebuilt runtime failed final audit.");
    }

    return RuntimeStateLoadResult::loaded(
        runtime, manifest,
        loadedBlockCount, mempoolLoad.loadedTransactionCount(),
        std::move(loadedArtifacts)
    );
}

} // namespace nodo::node
