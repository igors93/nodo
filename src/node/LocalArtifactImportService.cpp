#include "node/LocalArtifactImportService.hpp"

#include "consensus/QuorumCertificate.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/StateRootCalculator.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedArtifactValidator.hpp"
#include "node/FinalizedSlashingEvidenceAudit.hpp"
#include "node/FinalityArtifactValidator.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedTreasuryAudit.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "storage/AtomicFile.hpp"

#include <exception>
#include <filesystem>
#include <limits>
#include <sstream>

namespace nodo::node {

std::string artifactImportRejectionReasonToString(ArtifactImportRejectionReason reason) {
    switch (reason) {
        case ArtifactImportRejectionReason::NONE:                     return "NONE";
        case ArtifactImportRejectionReason::INVALID_CONFIG:           return "INVALID_CONFIG";
        case ArtifactImportRejectionReason::DIRECTORY_NOT_INITIALIZED:return "DIRECTORY_NOT_INITIALIZED";
        case ArtifactImportRejectionReason::GENESIS_MISMATCH:         return "GENESIS_MISMATCH";
        case ArtifactImportRejectionReason::DECODE_FAILED:            return "DECODE_FAILED";
        case ArtifactImportRejectionReason::INVALID_ARTIFACT:         return "INVALID_ARTIFACT";
        case ArtifactImportRejectionReason::HEIGHT_CONTINUITY_MISMATCH:return "HEIGHT_CONTINUITY_MISMATCH";
        case ArtifactImportRejectionReason::PREVIOUS_HASH_MISMATCH:   return "PREVIOUS_HASH_MISMATCH";
        case ArtifactImportRejectionReason::ARTIFACT_DIGEST_EMPTY:    return "ARTIFACT_DIGEST_EMPTY";
        case ArtifactImportRejectionReason::ARTIFACT_DIGEST_UNSTABLE: return "ARTIFACT_DIGEST_UNSTABLE";
        case ArtifactImportRejectionReason::SUPPLY_CONTINUITY_BREAK:  return "SUPPLY_CONTINUITY_BREAK";
        case ArtifactImportRejectionReason::ARTIFACT_VALIDATION_FAILED:return "ARTIFACT_VALIDATION_FAILED";
        case ArtifactImportRejectionReason::FINALITY_VALIDATION_FAILED:return "FINALITY_VALIDATION_FAILED";
        case ArtifactImportRejectionReason::REWARD_EVIDENCE_MISSING:  return "REWARD_EVIDENCE_MISSING";
        case ArtifactImportRejectionReason::TREASURY_DIGEST_MISMATCH: return "TREASURY_DIGEST_MISMATCH";
        case ArtifactImportRejectionReason::CONFLICTING_ARTIFACT:     return "CONFLICTING_ARTIFACT";
        case ArtifactImportRejectionReason::PERSIST_FAILED:           return "PERSIST_FAILED";
        default:                                                       return "INVALID_ARTIFACT";
    }
}

// ---- FinalizedArtifactImportResult ----

FinalizedArtifactImportResult::FinalizedArtifactImportResult()
    : m_accepted(false),
      m_rejectionReason(ArtifactImportRejectionReason::INVALID_ARTIFACT),
      m_detail("Uninitialized import result."),
      m_manifest() {}

FinalizedArtifactImportResult FinalizedArtifactImportResult::accepted(NodeRuntimeManifest manifest) {
    FinalizedArtifactImportResult r;
    r.m_accepted = true;
    r.m_rejectionReason = ArtifactImportRejectionReason::NONE;
    r.m_detail.clear();
    r.m_manifest = std::move(manifest);
    return r;
}

FinalizedArtifactImportResult FinalizedArtifactImportResult::rejected(
    ArtifactImportRejectionReason reason, std::string detail
) {
    FinalizedArtifactImportResult r;
    r.m_accepted = false;
    r.m_rejectionReason = reason;
    r.m_detail = std::move(detail);
    return r;
}

bool FinalizedArtifactImportResult::accepted() const { return m_accepted; }
ArtifactImportRejectionReason FinalizedArtifactImportResult::rejectionReason() const { return m_rejectionReason; }
const std::string& FinalizedArtifactImportResult::detail() const { return m_detail; }
const NodeRuntimeManifest& FinalizedArtifactImportResult::manifest() const { return m_manifest; }

std::string FinalizedArtifactImportResult::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedArtifactImportResult{"
        << "accepted=" << (m_accepted ? "true" : "false")
        << ";reason=" << artifactImportRejectionReasonToString(m_rejectionReason)
        << ";detail=" << m_detail
        << "}";
    return oss.str();
}

// ---- LocalArtifactImportService ----

namespace {

std::int64_t minimumFeeRawUnits(const config::GenesisConfig& genesis) {
    const std::uint64_t f = genesis.networkParameters().minimumFeeRawUnits();
    if (f > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(f);
}

std::uint64_t requiredVotingWeight(
    const config::GenesisConfig& genesis,
    const core::ValidatorRegistry& registry
) {
    return consensus::QuorumCertificateBuilder::requiredVotingWeight(
        registry.totalConsensusWeight(),
        genesis.networkParameters().quorumThresholdNumerator(),
        genesis.networkParameters().quorumThresholdDenominator()
    );
}

FinalizedArtifactImportResult importImpl(
    const NodeDataDirectoryConfig& targetDir,
    NodeRuntime& runtime,
    const config::GenesisConfig& genesisConfig,
    const FinalizedBlockArtifact& artifact,
    const std::string& rawContents,
    std::int64_t importedAt
) {
    if (!targetDir.isValid() || importedAt <= 0 ||
        importedAt == std::numeric_limits<std::int64_t>::max() ||
        !runtime.isValid() || !genesisConfig.isValid()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_CONFIG,
            "target directory, runtime, genesis or import timestamp is invalid"
        );
    }

    if (!NodeDataDirectory::isInitialized(targetDir)) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::DIRECTORY_NOT_INITIALIZED,
            "target data directory is not initialized"
        );
    }

    // Verify genesis identity matches.
    const NodeDataDirectoryReadResult manifestResult =
        NodeDataDirectory::loadManifest(targetDir);

    if (!manifestResult.loaded()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::DIRECTORY_NOT_INITIALIZED,
            "cannot read target manifest: " + manifestResult.reason()
        );
    }

    if (manifestResult.manifest().genesisConfigId() != genesisConfig.deterministicId()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::GENESIS_MISMATCH,
            "artifact genesis '" + genesisConfig.deterministicId() +
            "' does not match target directory genesis '" +
            manifestResult.manifest().genesisConfigId() + "'"
        );
    }

    if (runtime.config().genesisConfig().deterministicId() !=
        genesisConfig.deterministicId()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::GENESIS_MISMATCH,
            "runtime genesis does not match the import genesis"
        );
    }

    // Structural validity.
    if (!artifact.isValid()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_ARTIFACT,
            "artifact is not structurally valid"
        );
    }
    if (artifact.block().index() == std::numeric_limits<std::uint64_t>::max()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::HEIGHT_CONTINUITY_MISMATCH,
            "artifact height cannot have a successor"
        );
    }

    if (artifact.block().timestamp() > importedAt &&
        artifact.block().timestamp() - importedAt > 300) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_ARTIFACT,
            "artifact block timestamp is more than 300 seconds in the future"
        );
    }

    // An artifact file can be left behind when a prior import failed before
    // publishing the runtime snapshot. Its presence alone proves no commit.
    const std::filesystem::path targetPath =
        FinalizedBlockStore::blockFilePath(targetDir, artifact.block().index());

    if (std::filesystem::exists(targetPath)) {
        std::string existingContents;
        try {
            existingContents = storage::AtomicFile::readTextFile(targetPath);
        } catch (const std::exception& error) {
            return FinalizedArtifactImportResult::rejected(
                ArtifactImportRejectionReason::PERSIST_FAILED, error.what()
            );
        }

        if (existingContents != rawContents) {
            return FinalizedArtifactImportResult::rejected(
                ArtifactImportRejectionReason::CONFLICTING_ARTIFACT,
                "a different artifact already exists at height " +
                std::to_string(artifact.block().index())
            );
        }

        if (runtime.blockchain().latestBlock().index() >= artifact.block().index()) {
            bool applied = false;
            for (const core::Block& block : runtime.blockchain().blocks()) {
                if (block.index() == artifact.block().index()) {
                    applied = block.hash() == artifact.block().hash();
                    break;
                }
            }
            if (!applied) {
                return FinalizedArtifactImportResult::rejected(
                    ArtifactImportRejectionReason::CONFLICTING_ARTIFACT,
                    "stored artifact does not match the runtime chain at its height"
                );
            }
            if (!runtime.finalizationRegistry().isFinalizedBlock(
                    artifact.block().index(), artifact.block().hash())) {
                return FinalizedArtifactImportResult::rejected(
                    ArtifactImportRejectionReason::FINALITY_VALIDATION_FAILED,
                    "stored artifact has not been finalized in the runtime"
                );
            }
            NodeRuntime stagedRuntime = runtime;
            try {
                const ProtocolReplayState current =
                    ProtocolStateTransition::replayStateFromRuntime(
                        stagedRuntime, minimumFeeRawUnits(genesisConfig)
                    );
                const std::string currentRoot =
                    core::StateRootCalculator::calculateProtocolStateRoot(
                        current.accounts, protocolExecutionDomains(current.execution)
                    );
                if (currentRoot != stagedRuntime.blockchain().latestBlock().stateRoot()) {
                    throw std::logic_error(
                        "runtime protocol domains do not match the finalized tip"
                    );
                }
                if (stagedRuntime.blockchain().latestBlock().index() ==
                    std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error(
                        "runtime block height cannot advance without overflow"
                    );
                }
                const std::uint64_t nextHeight =
                    stagedRuntime.blockchain().latestBlock().index() + 1;
                if (stagedRuntime.consensusRoundManager().currentState().height() !=
                    nextHeight) {
                    const std::string proposer =
                        consensus::ProposerSchedule::selectProposer(
                            stagedRuntime.validatorRegistry(),
                            genesisConfig.networkParameters().chainId(),
                            nextHeight, 1
                        );
                    stagedRuntime.mutableConsensusRoundManager().advanceToHeight(
                        nextHeight, 1, proposer, importedAt + 1,
                        genesisConfig.networkParameters().targetBlockTimeSeconds()
                    );
                }
            } catch (const std::exception& error) {
                return FinalizedArtifactImportResult::rejected(
                    ArtifactImportRejectionReason::ARTIFACT_VALIDATION_FAILED,
                    error.what()
                );
            }
            const NodeDataDirectoryReadResult snapshot =
                NodeDataDirectory::writeRuntimeSnapshot(
                    targetDir, stagedRuntime, importedAt
                );
            if (!snapshot.loaded()) {
                return FinalizedArtifactImportResult::rejected(
                    ArtifactImportRejectionReason::PERSIST_FAILED, snapshot.reason()
                );
            }
            runtime = std::move(stagedRuntime);
            return FinalizedArtifactImportResult::accepted(snapshot.manifest());
        }
    }

    // Height continuity: artifact must be exactly the next block.
    const std::uint64_t currentHeight = runtime.blockchain().latestBlock().index();
    if (currentHeight == std::numeric_limits<std::uint64_t>::max()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::HEIGHT_CONTINUITY_MISMATCH,
            "runtime block height cannot advance without overflow"
        );
    }
    const std::uint64_t expectedHeight = currentHeight + 1;

    if (artifact.block().index() != expectedHeight) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::HEIGHT_CONTINUITY_MISMATCH,
            "expected height " + std::to_string(expectedHeight) +
            " but artifact has height " + std::to_string(artifact.block().index())
        );
    }

    // Previous hash continuity.
    const std::string currentHash = runtime.blockchain().latestBlock().hash();
    if (artifact.block().previousHash() != currentHash) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PREVIOUS_HASH_MISMATCH,
            "artifact previousHash '" + artifact.block().previousHash() +
            "' does not match current tip hash '" + currentHash + "'"
        );
    }

    // Artifact digest stability.
    const std::string firstDigest = artifact.artifactDigest();
    if (firstDigest.empty()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::ARTIFACT_DIGEST_EMPTY,
            "artifact at height " + std::to_string(artifact.block().index()) +
            " produced an empty digest — storage integrity cannot be verified"
        );
    }
    if (artifact.artifactDigest() != firstDigest) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::ARTIFACT_DIGEST_UNSTABLE,
            "artifact digest is non-deterministic at height " +
            std::to_string(artifact.block().index())
        );
    }

    // Reward evidence: every protection reward settlement must carry evidence.
    const RewardEvidenceAuditResult rewardAudit =
        ProtectionRewards::auditSettlementEvidence(
            artifact.protectionRewardSettlements()
        );
    if (!rewardAudit.isPassed()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::REWARD_EVIDENCE_MISSING,
            "reward evidence audit failed at height " +
            std::to_string(artifact.block().index()) + ": " + rewardAudit.reason()
        );
    }

    // Treasury section audit: validates spend records.
    const FinalizedTreasuryAuditResult treasuryAudit =
        FinalizedTreasuryAudit::auditArtifacts(0, {artifact});
    if (!treasuryAudit.passed()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::TREASURY_DIGEST_MISMATCH,
            "treasury audit failed at height " +
            std::to_string(artifact.block().index()) + ": " + treasuryAudit.reason()
        );
    }

    // Crypto context.
    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            manifestResult.manifest().networkName()
        );

    if (!cryptoContext.isValid()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_CONFIG,
            "invalid crypto context for network '" +
            manifestResult.manifest().networkName() + "': " +
            cryptoContext.rejectionReason()
        );
    }

    // Full artifact validation pipeline (includes finality, state, economic,
    // monetary, slashing, governance, validator lifecycle validators).
    FinalizedArtifactValidationContext validationContext(
        genesisConfig,
        runtime,
        cryptoContext,
        targetPath,
        requiredVotingWeight(genesisConfig, runtime.validatorRegistry()),
        minimumFeeRawUnits(genesisConfig)
    );

    const ArtifactValidationResult validation =
        FinalizedArtifactValidator::validate(validationContext, artifact);

    if (!validation.accepted()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::ARTIFACT_VALIDATION_FAILED,
            validation.reason()
        );
    }

    // Prepare the complete next runtime before publishing either the block file
    // or the live runtime. A failed replay cannot leave a partially applied tip.
    NodeRuntime stagedRuntime = runtime;
    try {
        const std::int64_t minFee = minimumFeeRawUnits(genesisConfig);
        const ProtocolReplayState previous =
            ProtocolStateTransition::replayStateFromRuntime(stagedRuntime, minFee);
        const ProtocolReplayState replayed =
            ProtocolStateTransition::replayBlock(
                genesisConfig,
                previous,
                artifact.block(),
                minFee,
                artifact.block().timestamp()
            );
        if (replayed.stateRoot != artifact.postStateRoot() ||
            replayed.receiptsRoot != artifact.block().receiptsRoot() ||
            replayed.execution.supply != artifact.supplyDelta().supplyAfter()) {
            throw std::logic_error(
                "Replayed commitments or supply differ from the artifact."
            );
        }

        FinalizedArtifactValidationContext stagedContext(
            genesisConfig, stagedRuntime, cryptoContext, targetPath,
            requiredVotingWeight(genesisConfig, stagedRuntime.validatorRegistry()),
            minFee
        );
        const ArtifactValidationResult finalization =
            FinalityArtifactValidator::applyFinalization(stagedContext, artifact);
        if (!finalization.accepted()) {
            return FinalizedArtifactImportResult::rejected(
                ArtifactImportRejectionReason::FINALITY_VALIDATION_FAILED,
                finalization.reason()
            );
        }

        stagedRuntime.mutableSupplyState().applyFinalizedDelta(artifact.supplyDelta());
        ProtocolStateTransition::applyReplayDomainsToRuntime(stagedRuntime, replayed);
        stagedRuntime.setCachedAccountStateAtTip(replayed.accounts);
        const FinalizedSlashingEvidenceAuditResult slashingAudit =
            FinalizedSlashingEvidenceAudit::auditBlockEffects(
                artifact.block(), stagedRuntime.validatorPenaltyLedger(),
                stagedRuntime.validatorRegistry(), stagedRuntime.stakingRegistry()
            );
        if (!slashingAudit.passed()) {
            throw std::logic_error(slashingAudit.reason());
        }
        const std::uint64_t nextHeight = artifact.block().index() + 1;
        if (!stagedRuntime.mutableValidatorSetHistory().recordSet(
                nextHeight, stagedRuntime.validatorRegistry())) {
            throw std::logic_error("Validator set history conflict after import.");
        }
        if (!artifact.postStateRoot().empty()) {
            stagedRuntime.mutableStatePruner().recordStateRoot(
                artifact.block().index(), artifact.postStateRoot()
            );
        }
        constexpr std::uint64_t nextRound = 1;
        const std::string nextProposer = consensus::ProposerSchedule::selectProposer(
            stagedRuntime.validatorRegistry(),
            genesisConfig.networkParameters().chainId(), nextHeight, nextRound
        );
        stagedRuntime.mutableConsensusRoundManager().advanceToHeight(
            nextHeight, nextRound, nextProposer, importedAt + 1,
            genesisConfig.networkParameters().targetBlockTimeSeconds()
        );
    } catch (const std::exception& e) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::SUPPLY_CONTINUITY_BREAK,
            std::string("protocol state preparation failed at height ") +
            std::to_string(artifact.block().index()) + ": " + e.what()
        );
    }

    try {
        std::filesystem::create_directories(targetDir.blocksDirectoryPath());
        if (!std::filesystem::exists(targetPath)) {
            storage::AtomicFile::writeTextFile(targetPath, rawContents);
        }
    } catch (const std::exception& e) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PERSIST_FAILED,
            std::string("failed to write artifact file: ") + e.what()
        );
    }

    // The manifest is published last. An orphaned block file is safe to retry.
    const NodeDataDirectoryReadResult snapshot =
        NodeDataDirectory::writeRuntimeSnapshot(targetDir, stagedRuntime, importedAt);

    if (!snapshot.loaded()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PERSIST_FAILED,
            snapshot.reason()
        );
    }

    if (snapshot.manifest().latestBlockHeight() != artifact.block().index() ||
        snapshot.manifest().latestBlockHash() != artifact.block().hash() ||
        snapshot.manifest().latestStateRoot() != artifact.postStateRoot()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PERSIST_FAILED,
            "published manifest does not match imported block commitments"
        );
    }

    runtime = std::move(stagedRuntime);
    return FinalizedArtifactImportResult::accepted(snapshot.manifest());
}

} // namespace

FinalizedArtifactImportResult LocalArtifactImportService::importArtifactFromFile(
    const NodeDataDirectoryConfig& targetDir,
    NodeRuntime& runtime,
    const config::GenesisConfig& genesisConfig,
    const std::filesystem::path& sourceArtifactPath,
    std::int64_t importedAt
) {
    if (sourceArtifactPath.empty() || !std::filesystem::exists(sourceArtifactPath)) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_CONFIG,
            "source artifact path does not exist: " + sourceArtifactPath.string()
        );
    }

    std::string rawContents;
    FinalizedBlockArtifact artifact;

    try {
        rawContents = storage::AtomicFile::readTextFile(sourceArtifactPath);
        artifact = FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(rawContents);
    } catch (const std::exception& e) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::DECODE_FAILED,
            std::string("failed to decode source artifact: ") + e.what()
        );
    }

    return importImpl(
        targetDir, runtime, genesisConfig, artifact, rawContents, importedAt
    );
}

FinalizedArtifactImportResult LocalArtifactImportService::importArtifact(
    const NodeDataDirectoryConfig& targetDir,
    NodeRuntime& runtime,
    const config::GenesisConfig& genesisConfig,
    const FinalizedBlockArtifact& artifact,
    const std::string& rawArtifactContents,
    std::int64_t importedAt
) {
    FinalizedBlockArtifact decoded;
    try {
        decoded = FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(
            rawArtifactContents
        );
    } catch (const std::exception& error) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::DECODE_FAILED, error.what()
        );
    }
    if (!artifact.isValid() ||
        artifact.block().hash() != decoded.block().hash() ||
        artifact.artifactDigest() != decoded.artifactDigest() ||
        artifact.quorumCertificate().serialize() !=
            decoded.quorumCertificate().serialize()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_ARTIFACT,
            "decoded artifact does not match the supplied artifact"
        );
    }
    return importImpl(
        targetDir, runtime, genesisConfig, decoded, rawArtifactContents, importedAt
    );
}

} // namespace nodo::node
