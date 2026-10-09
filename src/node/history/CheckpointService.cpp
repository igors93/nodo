#include "node/history/CheckpointService.hpp"

#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"
#include "node/history/StorageMigration.hpp"
#include "utils/Logger.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t checkedMinimumFee(const config::GenesisConfig &genesisConfig) {
  const std::uint64_t raw =
      genesisConfig.networkParameters().minimumFeeRawUnits();
  if (raw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("Minimum fee exceeds the Amount range.");
  }
  return static_cast<std::int64_t>(raw);
}

CheckpointCreationResult creation(CheckpointCreationStatus status,
                                  std::string reason) {
  CheckpointCreationResult result;
  result.status = status;
  result.reason = std::move(reason);
  return result;
}

void requireAccepted(const CheckpointVerificationResult &result,
                     const std::string &stage) {
  if (!result.isAccepted()) {
    throw std::logic_error("Locally built checkpoint failed " + stage + " (" +
                           checkpointVerificationStatusToString(
                               result.status()) +
                           "): " + result.reason());
  }
}

} // namespace

std::string checkpointCreationStatusToString(CheckpointCreationStatus status) {
  switch (status) {
  case CheckpointCreationStatus::CREATED:
    return "CREATED";
  case CheckpointCreationStatus::ALREADY_PRESENT:
    return "ALREADY_PRESENT";
  case CheckpointCreationStatus::NOT_DUE:
    return "NOT_DUE";
  case CheckpointCreationStatus::SKIPPED:
    return "SKIPPED";
  case CheckpointCreationStatus::CONFLICT:
    return "CONFLICT";
  case CheckpointCreationStatus::FAILED:
    return "FAILED";
  }
  return "FAILED";
}

archive::ArchivedBlockSource
CheckpointService::runtimeBlockSource(const NodeRuntime &runtime) {
  return [&runtime](std::uint64_t height) -> std::optional<archive::ArchivedBlock> {
    const std::optional<core::Block> block =
        runtime.blockchain().blockByHeight(height);
    if (!block || block->index() != height || block->previousHash() == "SNAPSHOT") {
      return std::nullopt;
    }
    return archive::ArchivedBlock{block->hash(), block->serialize()};
  };
}

archive::ArchivedBlockSource
CheckpointService::blockFileSource(const NodeDataDirectoryConfig &directory) {
  return [directory](std::uint64_t height) -> std::optional<archive::ArchivedBlock> {
    const std::filesystem::path path =
        FinalizedBlockStore::blockFilePath(directory, height);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
      return std::nullopt;
    }
    const core::Block block = FinalizedBlockArtifactCodec::readBlockFile(path);
    if (block.index() != height) {
      return std::nullopt;
    }
    return archive::ArchivedBlock{block.hash(), block.serialize()};
  };
}

CheckpointArtifacts CheckpointService::build(
    const config::GenesisConfig &genesisConfig,
    const config::HistoryParameters &parameters, std::uint64_t height,
    const std::string &blockHash, const std::string &previousBlockHash,
    std::int64_t blockTimestamp, const ProtocolReplayState &replayState,
    const consensus::QuorumCertificate &quorumCertificate,
    const HistoryStore &store, const archive::ArchivedBlockSource &blockSource) {
  if (!parameters.isValid() || !parameters.isCheckpointHeight(height)) {
    throw std::invalid_argument("Height is not a scheduled checkpoint height.");
  }
  const config::NetworkParameters &network = genesisConfig.networkParameters();
  if (parameters.networkName() != network.networkName()) {
    throw std::invalid_argument("History parameters belong to another network.");
  }

  const FullProtocolStateSnapshot snapshot =
      FullProtocolStateSnapshot::fromReplayState(
          genesisConfig, height, blockHash, blockTimestamp, replayState);
  if (!snapshot.state().verifiesProtocolStateRoot()) {
    throw std::logic_error(
        "Local protocol state does not reproduce the finalized state root.");
  }

  CheckpointArtifacts artifacts;
  std::vector<archive::ArchiveSegmentCommitment> commitments;
  const std::uint64_t sealed = parameters.sealedSegmentCount(height);
  for (std::uint64_t index = 0; index < sealed; ++index) {
    std::optional<archive::ArchiveSegmentCommitment> stored =
        store.loadSegmentCommitment(index);
    if (!stored) {
      const archive::ArchiveSegmentIndex built =
          archive::ArchiveSegmentBuilder::build(parameters, network.chainId(),
                                                index, blockSource);
      artifacts.newSegments.push_back(built.commitment());
      stored = built.commitment();
    }
    if (!stored->matchesParameters(parameters) ||
        stored->chainId() != network.chainId()) {
      throw std::logic_error("Stored archive segment commitment " +
                             std::to_string(index) +
                             " does not match the network parameters.");
    }
    commitments.push_back(*stored);
  }

  FinalizedStateCheckpointFields fields;
  fields.networkName = network.networkName();
  fields.chainId = network.chainId();
  fields.genesisConfigId = genesisConfig.deterministicId();
  fields.protocolVersion = network.protocolVersion();
  fields.historyParametersId = parameters.deterministicId();
  fields.height = height;
  fields.epoch = ValidatorLifecycle::epochIndexForBlock(height);
  fields.blockHash = blockHash;
  fields.previousBlockHash = previousBlockHash;
  fields.blockTimestamp = blockTimestamp;
  fields.stateRoot = snapshot.state().stateRoot();
  fields.accountsRoot = snapshot.state().accountRoot();
  fields.validatorSetRoot =
      snapshot.consensusWindow().setAt(height).validatorSetRoot();
  fields.nextValidatorSetRoot =
      snapshot.consensusWindow().setAt(height + 1).validatorSetRoot();
  fields.consensusContextDigest = snapshot.consensusContextDigest();
  artifacts.snapshotBytes = snapshot.encode();
  fields.snapshotDigest = FullProtocolStateSnapshot::digestOfEncoding(
      artifacts.snapshotBytes, parameters.snapshotChunkBytes());
  fields.archiveSealedSegmentCount = sealed;
  fields.archiveIndexRoot = archive::ArchiveIndex::root(commitments);
  if (height > parameters.checkpointIntervalBlocks()) {
    const std::uint64_t previousHeight =
        height - parameters.checkpointIntervalBlocks();
    const std::optional<FinalizedStateCheckpoint> previous =
        store.loadCheckpoint(previousHeight);
    if (!previous) {
      throw std::runtime_error("Previous checkpoint at height " +
                               std::to_string(previousHeight) +
                               " is missing; run `nodo checkpoint backfill`.");
    }
    fields.previousCheckpointHeight = previousHeight;
    fields.previousCheckpointId = previous->checkpointId();
  }

  artifacts.checkpoint =
      FinalizedStateCheckpoint(std::move(fields), quorumCertificate);
  requireAccepted(FinalizedStateCheckpointVerifier::verifyIdentity(
                      artifacts.checkpoint, genesisConfig, parameters),
                  "identity verification");
  requireAccepted(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                      snapshot, artifacts.checkpoint, genesisConfig, parameters),
                  "snapshot verification");
  requireAccepted(FinalizedStateCheckpointVerifier::verifyCertificate(
                      artifacts.checkpoint,
                      snapshot.consensusWindow().setAt(height),
                      crypto::ProtocolCryptoContext::fromNetworkName(
                          network.networkName())),
                  "certificate verification");
  return artifacts;
}

CheckpointCreationResult
CheckpointService::persist(const HistoryStore &store,
                           const CheckpointArtifacts &artifacts) {
  for (const archive::ArchiveSegmentCommitment &commitment :
       artifacts.newSegments) {
    const HistoryWriteResult write = store.saveSegmentCommitment(commitment);
    if (!write.stored()) {
      return creation(write.status == HistoryWriteStatus::CONFLICT
                          ? CheckpointCreationStatus::CONFLICT
                          : CheckpointCreationStatus::FAILED,
                      "archive segment " +
                          std::to_string(commitment.segmentIndex()) + ": " +
                          write.reason);
    }
  }
  const std::uint64_t height = artifacts.checkpoint.height();
  const HistoryWriteResult snapshot =
      store.saveSnapshot(height, artifacts.snapshotBytes);
  if (!snapshot.stored()) {
    return creation(snapshot.status == HistoryWriteStatus::CONFLICT
                        ? CheckpointCreationStatus::CONFLICT
                        : CheckpointCreationStatus::FAILED,
                    "checkpoint snapshot: " + snapshot.reason);
  }
  const HistoryWriteResult checkpoint =
      store.saveCheckpoint(artifacts.checkpoint);
  CheckpointCreationResult result;
  result.checkpoint = artifacts.checkpoint;
  switch (checkpoint.status) {
  case HistoryWriteStatus::WRITTEN:
    result.status = CheckpointCreationStatus::CREATED;
    break;
  case HistoryWriteStatus::ALREADY_PRESENT:
    result.status = CheckpointCreationStatus::ALREADY_PRESENT;
    break;
  case HistoryWriteStatus::CONFLICT:
    result.status = CheckpointCreationStatus::CONFLICT;
    break;
  case HistoryWriteStatus::IO_ERROR:
    result.status = CheckpointCreationStatus::FAILED;
    break;
  }
  result.reason = checkpoint.reason;
  return result;
}

CheckpointCreationResult
CheckpointService::onBlockFinalized(const NodeDataDirectoryConfig &directory,
                                    const NodeRuntime &runtime) {
  try {
    if (!runtime.isValid() || runtime.blockchain().empty()) {
      return creation(CheckpointCreationStatus::SKIPPED, "runtime is invalid");
    }
    const config::GenesisConfig &genesisConfig =
        runtime.config().genesisConfig();
    const config::HistoryParameters parameters =
        config::HistoryParameters::forNetwork(
            genesisConfig.networkParameters().networkName());
    const core::Block &tip = runtime.blockchain().latestBlock();
    if (!parameters.isCheckpointHeight(tip.index())) {
      return creation(CheckpointCreationStatus::NOT_DUE, "");
    }
    if (!StorageMigration::supportsHistoryLayout(directory)) {
      return creation(CheckpointCreationStatus::SKIPPED,
                      "storage schema predates the history layout; run "
                      "`nodo storage migrate`");
    }
    const HistoryStore store(directory);
    if (tip.index() > parameters.checkpointIntervalBlocks() &&
        !store.loadCheckpoint(tip.index() -
                              parameters.checkpointIntervalBlocks())) {
      return creation(CheckpointCreationStatus::SKIPPED,
                      "previous checkpoint is missing; run `nodo checkpoint "
                      "backfill`");
    }
    const consensus::FinalizedBlockRecord *record =
        runtime.finalizationRegistry().recordForHeight(tip.index());
    if (record == nullptr || !record->matchesBlock(tip)) {
      return creation(CheckpointCreationStatus::FAILED,
                      "finalized record for the checkpoint block is missing");
    }
    const ProtocolReplayState replay =
        ProtocolStateTransition::replayStateFromRuntime(
            runtime, checkedMinimumFee(genesisConfig));
    const CheckpointArtifacts artifacts =
        build(genesisConfig, parameters, tip.index(), tip.hash(),
              tip.previousHash(), tip.timestamp(), replay,
              record->quorumCertificate(), store, runtimeBlockSource(runtime));
    CheckpointCreationResult result = persist(store, artifacts);
    if (result.status == CheckpointCreationStatus::CREATED ||
        result.status == CheckpointCreationStatus::ALREADY_PRESENT) {
      const CheckpointVerificationResult verified =
          verifyStored(directory, genesisConfig, tip.index());
      if (!verified.isAccepted()) {
        return creation(CheckpointCreationStatus::FAILED,
                        "stored checkpoint failed local verification: " +
                            verified.reason());
      }
    }
    if (result.status == CheckpointCreationStatus::CONFLICT) {
      utils::log(utils::LogLevel::ERROR, "CheckpointService")
          << "conflicting checkpoint at height " << tip.index() << ": "
          << result.reason << std::endl;
    }
    return result;
  } catch (const std::exception &error) {
    utils::log(utils::LogLevel::WARN, "CheckpointService")
        << "checkpoint creation failed: " << error.what() << std::endl;
    return creation(CheckpointCreationStatus::FAILED, error.what());
  }
}

CheckpointBackfillResult
CheckpointService::backfill(const NodeDataDirectoryConfig &directory,
                            const config::GenesisConfig &genesisConfig) {
  CheckpointBackfillResult result;
  try {
    if (!StorageMigration::supportsHistoryLayout(directory)) {
      result.reason = "storage schema predates the history layout; run "
                      "`nodo storage migrate` first";
      return result;
    }
    const NodeDataDirectoryReadResult manifest =
        NodeDataDirectory::loadManifest(directory);
    if (!manifest.loaded() ||
        manifest.manifest().genesisConfigId() !=
            genesisConfig.deterministicId()) {
      result.reason = "data directory manifest is missing or for another "
                      "genesis";
      return result;
    }
    const config::HistoryParameters parameters =
        config::HistoryParameters::forNetwork(
            genesisConfig.networkParameters().networkName());
    const HistoryStore store(directory);
    const std::int64_t minimumFee = checkedMinimumFee(genesisConfig);
    ProtocolReplayState replay =
        ProtocolStateTransition::initialReplayState(genesisConfig);
    const archive::ArchivedBlockSource source = blockFileSource(directory);

    for (std::uint64_t height = 1;
         height <= manifest.manifest().latestBlockHeight(); ++height) {
      const FinalizedBlockArtifact artifact =
          FinalizedBlockArtifactCodec::readBlockArtifactFile(
              FinalizedBlockStore::blockFilePath(directory, height));
      const core::Block &block = artifact.block();
      replay = ProtocolStateTransition::replayBlock(
          genesisConfig, replay, block, minimumFee, block.timestamp());
      if (block.index() != height || replay.stateRoot != block.stateRoot()) {
        result.reason = "replay diverged from the stored chain at height " +
                        std::to_string(height);
        return result;
      }
      result.replayedHeight = height;
      if (!parameters.isCheckpointHeight(height)) {
        continue;
      }
      const CheckpointArtifacts artifacts =
          build(genesisConfig, parameters, height, block.hash(),
                block.previousHash(), block.timestamp(), replay,
                artifact.finalizedRecord().quorumCertificate(), store, source);
      const CheckpointCreationResult persisted = persist(store, artifacts);
      if (persisted.status == CheckpointCreationStatus::CREATED) {
        ++result.createdCheckpoints;
      } else if (persisted.status == CheckpointCreationStatus::ALREADY_PRESENT) {
        ++result.existingCheckpoints;
      } else {
        result.reason = "checkpoint " + std::to_string(height) + " " +
                        checkpointCreationStatusToString(persisted.status) +
                        ": " + persisted.reason;
        return result;
      }
      const CheckpointVerificationResult verified =
          verifyStored(directory, genesisConfig, height);
      if (!verified.isAccepted()) {
        result.reason = "stored checkpoint " + std::to_string(height) +
                        " failed verification: " + verified.reason();
        return result;
      }
    }
    result.success = true;
    return result;
  } catch (const std::exception &error) {
    result.success = false;
    result.reason = error.what();
    return result;
  }
}

CheckpointVerificationResult
CheckpointService::verifyStored(const NodeDataDirectoryConfig &directory,
                                const config::GenesisConfig &genesisConfig,
                                std::uint64_t height) {
  using Status = CheckpointVerificationStatus;
  try {
    const config::HistoryParameters parameters =
        config::HistoryParameters::forNetwork(
            genesisConfig.networkParameters().networkName());
    const HistoryStore store(directory);
    const std::optional<FinalizedStateCheckpoint> checkpoint =
        store.loadCheckpoint(height);
    if (!checkpoint) {
      return CheckpointVerificationResult::rejected(
          Status::MALFORMED, "no checkpoint is stored at this height");
    }
    CheckpointVerificationResult identity =
        FinalizedStateCheckpointVerifier::verifyIdentity(
            *checkpoint, genesisConfig, parameters);
    if (!identity.isAccepted()) {
      return identity;
    }
    const std::optional<FullProtocolStateSnapshot> snapshot =
        store.loadSnapshot(height, parameters.maxSnapshotBytes());
    if (!snapshot) {
      return CheckpointVerificationResult::rejected(
          Status::SNAPSHOT_DIGEST_MISMATCH,
          "the checkpoint snapshot is missing");
    }
    CheckpointVerificationResult state =
        FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
            *snapshot, *checkpoint, genesisConfig, parameters);
    if (!state.isAccepted()) {
      return state;
    }
    CheckpointVerificationResult certificate =
        FinalizedStateCheckpointVerifier::verifyCertificate(
            *checkpoint, snapshot->consensusWindow().setAt(height),
            crypto::ProtocolCryptoContext::fromNetworkName(
                genesisConfig.networkParameters().networkName()));
    if (!certificate.isAccepted()) {
      return certificate;
    }
    const FinalizedStateCheckpointFields &fields = checkpoint->fields();
    if (fields.previousCheckpointHeight != 0) {
      const auto previous = store.loadCheckpoint(fields.previousCheckpointHeight);
      if (!previous) {
        return CheckpointVerificationResult::rejected(
            Status::LINK_MISMATCH, "previous scheduled checkpoint is missing");
      }
      CheckpointVerificationResult link =
          FinalizedStateCheckpointVerifier::verifyLink(*checkpoint, *previous);
      if (!link.isAccepted()) {
        return link;
      }
      const CheckpointVerificationResult previousIdentity =
          FinalizedStateCheckpointVerifier::verifyIdentity(
              *previous, genesisConfig, parameters);
      if (!previousIdentity.isAccepted()) {
        return previousIdentity;
      }
      const CheckpointVerificationResult previousCertificate =
          FinalizedStateCheckpointVerifier::verifyCertificate(
              *previous,
              snapshot->consensusWindow().setAt(fields.previousCheckpointHeight),
              crypto::ProtocolCryptoContext::fromNetworkName(
                  genesisConfig.networkParameters().networkName()));
      if (!previousCertificate.isAccepted()) {
        return previousCertificate;
      }
    }
    const auto commitments =
        store.loadSegmentCommitments(fields.archiveSealedSegmentCount);
    if (!commitments ||
        archive::ArchiveIndex::root(*commitments) != fields.archiveIndexRoot) {
      return CheckpointVerificationResult::rejected(
          Status::MALFORMED,
          "local archive segment commitments are missing or do not match the checkpoint");
    }
    for (const archive::ArchiveSegmentCommitment &segment : *commitments) {
      if (segment.chainId() != fields.chainId ||
          !segment.matchesParameters(parameters)) {
        return CheckpointVerificationResult::rejected(
            Status::MALFORMED,
            "local archive segment geometry or chain identity is invalid");
      }
    }
    return CheckpointVerificationResult::accepted();
  } catch (const std::exception &error) {
    return CheckpointVerificationResult::rejected(Status::MALFORMED,
                                                  error.what());
  }
}

} // namespace nodo::node
