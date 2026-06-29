#include "node/LocalArtifactImportService.hpp"

#include "consensus/QuorumCertificate.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedArtifactValidator.hpp"
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
    if (!targetDir.isValid()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_CONFIG,
            "target data directory config is invalid"
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

    // Structural validity.
    if (!artifact.isValid()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::INVALID_ARTIFACT,
            "artifact is not structurally valid"
        );
    }

    // Idempotency check: if the artifact is already on disk at this height,
    // verify it matches and return success without re-validating or re-applying.
    // This check must happen before height continuity to support repeated imports.
    const std::filesystem::path targetPath =
        FinalizedBlockStore::blockFilePath(targetDir, artifact.block().index());

    if (std::filesystem::exists(targetPath)) {
        const std::string existingContents =
            storage::AtomicFile::readTextFile(targetPath);

        if (existingContents != rawContents) {
            return FinalizedArtifactImportResult::rejected(
                ArtifactImportRejectionReason::CONFLICTING_ARTIFACT,
                "a different artifact already exists at height " +
                std::to_string(artifact.block().index())
            );
        }

        // Identical artifact already stored — return the current manifest.
        const NodeDataDirectoryReadResult snapshot =
            NodeDataDirectory::writeRuntimeSnapshot(targetDir, runtime, importedAt);
        if (!snapshot.loaded()) {
            return FinalizedArtifactImportResult::rejected(
                ArtifactImportRejectionReason::PERSIST_FAILED,
                snapshot.reason()
            );
        }
        return FinalizedArtifactImportResult::accepted(snapshot.manifest());
    }

    // Height continuity: artifact must be exactly the next block.
    const std::uint64_t currentHeight = runtime.blockchain().latestBlock().index();
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

    // Write artifact to disk BEFORE mutating runtime state.
    try {
        std::filesystem::create_directories(targetDir.blocksDirectoryPath());
        storage::AtomicFile::writeTextFile(targetPath, rawContents);
    } catch (const std::exception& e) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PERSIST_FAILED,
            std::string("failed to write artifact file: ") + e.what()
        );
    }

    // Apply finalization to runtime blockchain (appends block, updates state).
    const ArtifactValidationResult finalization =
        FinalityArtifactValidator::applyFinalization(validationContext, artifact);

    if (!finalization.accepted()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::FINALITY_VALIDATION_FAILED,
            finalization.reason()
        );
    }

    try {
        const ProtocolExecutionState replayed =
            ProtocolStateTransition::replayFinalizedBlockDomains(
                runtime, artifact.block()
            );
        if (replayed.supply != artifact.supplyDelta().supplyAfter()) {
            throw std::logic_error(
                "Replayed transaction supply differs from persisted delta."
            );
        }
        runtime.mutableSupplyState().applyFinalizedDelta(artifact.supplyDelta());
        runtime.mutableGovernanceExecutor() = replayed.governance;
        runtime.mutableValidatorRegistry() = replayed.validators;
        runtime.mutableValidatorPenaltyLedger() = replayed.penaltyLedger;
        runtime.mutableStakingRegistry() = replayed.staking;
        runtime.invalidateAccountStateCache();
    } catch (const std::exception& e) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::SUPPLY_CONTINUITY_BREAK,
            std::string("supply continuity break at height ") +
            std::to_string(artifact.block().index()) + ": " + e.what()
        );
    }

    if (!runtime.mutableValidatorSetHistory().recordSet(
            artifact.block().index() + 1, runtime.validatorRegistry()
        )) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::FINALITY_VALIDATION_FAILED,
            "Validator set history conflict while importing block " +
            std::to_string(artifact.block().index())
        );
    }

    if (!artifact.postStateRoot().empty()) {
        runtime.mutableStatePruner().recordStateRoot(
            artifact.block().index(),
            artifact.postStateRoot()
        );
    }

    // Update manifest to reflect the new latest block.
    const NodeDataDirectoryReadResult snapshot =
        NodeDataDirectory::writeRuntimeSnapshot(targetDir, runtime, importedAt);

    if (!snapshot.loaded()) {
        return FinalizedArtifactImportResult::rejected(
            ArtifactImportRejectionReason::PERSIST_FAILED,
            snapshot.reason()
        );
    }

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
    return importImpl(
        targetDir, runtime, genesisConfig, artifact, rawArtifactContents, importedAt
    );
}

} // namespace nodo::node
