#include "app/CommandLineInterface.hpp"

#include "archive/ArchivalChallenge.hpp"
#include "archive/ArchivalProof.hpp"
#include "archive/ArchiveProvider.hpp"
#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeStartupService.hpp"
#include "node/history/CheckpointBootstrap.hpp"
#include "node/history/CheckpointService.hpp"
#include "node/history/HistoryPruningEngine.hpp"
#include "node/history/HistoryStore.hpp"
#include "node/history/StorageMigration.hpp"
#include "node/history/StorageStatus.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/JsonText.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace nodo::app {

namespace {

using utils::jsonString;

struct DirectoryContext {
  node::NodeDataDirectoryConfig directory;
  node::NodeRuntimeManifest manifest;
  config::GenesisConfig genesis;
  config::HistoryParameters parameters;
};

CommandLineResult failure(const std::string &message) {
  return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                    message + "\n");
}

// Resolves the data directory, its manifest, its pinned genesis and the
// network's history parameters. Never trusts the directory blindly: the
// genesis must match the manifest.
std::optional<DirectoryContext> openDirectory(const CommandLineOptions &options,
                                              std::string &error) {
  const node::NodeDataDirectoryConfig directory(options.dataDirectory);
  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directory);
  if (!manifest.loaded()) {
    error = manifest.reason();
    return std::nullopt;
  }
  const config::GenesisLookupResult genesis =
      node::RuntimeStartupService::resolveGenesis(
          manifest.manifest().networkName(), options.dataDirectory,
          options.genesisFile);
  if (!genesis.found()) {
    error = "Cannot resolve genesis: " + genesis.reason();
    return std::nullopt;
  }
  if (genesis.genesis().deterministicId() !=
      manifest.manifest().genesisConfigId()) {
    error = "Data directory genesis does not match the resolved genesis.";
    return std::nullopt;
  }
  try {
    return DirectoryContext{directory, manifest.manifest(), genesis.genesis(),
                            config::HistoryParameters::forNetwork(
                                manifest.manifest().networkName())};
  } catch (const std::exception &exception) {
    error = exception.what();
    return std::nullopt;
  }
}

std::optional<node::NodeStorageMode> modeFromOptions(const CommandLineOptions &options) {
  return node::nodeStorageModeFromString(options.pruningMode);
}

node::PruningOptions pruningOptions(const CommandLineOptions &options,
                                    node::NodeStorageMode mode) {
  node::PruningOptions pruning;
  pruning.mode = mode;
  pruning.retentionBlocks = options.retainBlocks;
  pruning.retainedCheckpointSnapshots = options.retainSnapshots;
  pruning.dryRun = options.dryRun;
  return pruning;
}

std::string describePlan(const node::PruningRunResult &run) {
  const node::RetentionPlan &plan = run.plan;
  std::ostringstream out;
  out << "Mode: " << node::nodeStorageModeToString(plan.mode) << "\n"
      << "Tip height: " << plan.tipHeight << "\n"
      << "Verified checkpoint base: "
      << (plan.baseCheckpointHeight ? std::to_string(*plan.baseCheckpointHeight)
                                    : std::string("NONE"))
      << "\n"
      << "Block bodies safe below height: " << plan.blockBodySafeBelowHeight
      << "\n"
      << "Block body pruning: "
      << (plan.blockBodyPruningAllowed ? "ALLOWED" : "BLOCKED") << "\n";
  for (const std::string &blocker : plan.blockBodyBlockers) {
    out << "  blocked: " << blocker << "\n";
  }
  out << "Checkpoint snapshots to remove: " << plan.checkpointSnapshotsToPrune.size()
      << "\n"
      << "Legacy fast-sync snapshots to remove: "
      << plan.legacySnapshotsToPrune.size() << "\n"
      << "Retention rules:\n";
  for (const node::RetentionRule &rule : plan.rules) {
    out << "  " << node::retentionCategoryToString(rule.category) << ": "
        << (rule.permanent ? "permanent" : "prune below " +
                                               std::to_string(rule.pruneBelowHeight))
        << " - " << rule.rule << "\n";
  }
  return out.str();
}

CommandLineResult checkpointStatus(const CommandLineOptions &options,
                                   const DirectoryContext &context) {
  const node::StorageStatusReport report =
      node::StorageStatusReport::collect(context.directory);
  if (options.outputJson) {
    return CommandLineResult::success(report.serializeJson() + "\n");
  }
  std::ostringstream out;
  out << "Checkpoint interval: " << context.parameters.checkpointIntervalBlocks()
      << " blocks\n"
      << "Checkpoints: " << report.checkpointCount << "\n"
      << "Latest checkpoint height: " << report.latestCheckpointHeight << "\n"
      << "Latest checkpoint id: "
      << (report.latestCheckpointId.empty() ? "NONE" : report.latestCheckpointId)
      << "\n"
      << "Checkpoint age: " << report.checkpointAgeBlocks << " blocks\n"
      << "Next checkpoint height: " << report.nextCheckpointHeight << "\n"
      << "Conflicting checkpoints preserved: " << report.checkpointConflicts
      << "\n";
  return CommandLineResult::success(out.str());
}

CommandLineResult checkpointList(const CommandLineOptions &options,
                                 const DirectoryContext &context) {
  const node::HistoryStore store(context.directory);
  const std::vector<std::uint64_t> snapshots = store.snapshotHeights();
  std::ostringstream out;
  if (options.outputJson) {
    out << "{\"checkpoints\":[";
  }
  bool first = true;
  for (const std::uint64_t height : store.checkpointHeights()) {
    std::string id = "CORRUPT";
    std::string stateRoot;
    try {
      if (const auto checkpoint = store.loadCheckpoint(height)) {
        id = checkpoint->checkpointId();
        stateRoot = checkpoint->fields().stateRoot;
      }
    } catch (const std::exception &) {
    }
    const bool hasSnapshot =
        std::find(snapshots.begin(), snapshots.end(), height) != snapshots.end();
    if (options.outputJson) {
      out << (first ? "" : ",") << "{\"height\":" << height
          << ",\"checkpointId\":" << jsonString(id)
          << ",\"stateRoot\":" << jsonString(stateRoot)
          << ",\"snapshot\":" << (hasSnapshot ? "true" : "false") << "}";
    } else {
      out << height << " " << id << " snapshot=" << (hasSnapshot ? "yes" : "pruned")
          << "\n";
    }
    first = false;
  }
  if (options.outputJson) {
    out << "]}\n";
  } else if (first) {
    out << "No checkpoints yet.\n";
  }
  return CommandLineResult::success(out.str());
}

CommandLineResult checkpointShow(const CommandLineOptions &options,
                                 const DirectoryContext &context) {
  const node::HistoryStore store(context.directory);
  std::uint64_t height = options.height;
  if (!options.heightProvided) {
    const std::vector<std::uint64_t> heights = store.checkpointHeights();
    if (heights.empty()) {
      return failure("No checkpoints yet.");
    }
    height = heights.back();
  }
  const auto checkpoint = store.loadCheckpoint(height);
  if (!checkpoint) {
    return failure("No checkpoint at height " + std::to_string(height) + ".");
  }
  return CommandLineResult::success(checkpoint->serializeJson() + "\n");
}

CommandLineResult checkpointVerify(const CommandLineOptions &options,
                                   const DirectoryContext &context) {
  const node::HistoryStore store(context.directory);
  std::vector<std::uint64_t> heights = store.checkpointHeights();
  if (options.heightProvided) {
    heights = {options.height};
  }
  if (heights.empty()) {
    return failure("No checkpoints to verify.");
  }
  std::ostringstream out;
  bool allAccepted = true;
  for (const std::uint64_t height : heights) {
    const node::CheckpointVerificationResult result =
        node::CheckpointService::verifyStored(context.directory, context.genesis,
                                              height);
    out << "Checkpoint " << height << ": "
        << node::checkpointVerificationStatusToString(result.status());
    if (!result.isAccepted()) {
      out << " (" << result.reason() << ")";
      allAccepted = false;
    }
    out << "\n";
  }
  return allAccepted ? CommandLineResult::success(out.str())
                     : failure(out.str() + "Checkpoint verification failed.");
}

CommandLineResult checkpointBackfill(const DirectoryContext &context) {
  const node::CheckpointBackfillResult result =
      node::CheckpointService::backfill(context.directory, context.genesis);
  if (!result.success) {
    return failure("Checkpoint backfill failed: " + result.reason);
  }
  std::ostringstream out;
  out << "Replayed finalized blocks: " << result.replayedHeight << "\n"
      << "Created checkpoints: " << result.createdCheckpoints << "\n"
      << "Already present: " << result.existingCheckpoints << "\n";
  return CommandLineResult::success(out.str());
}

// Verifies a checkpoint, snapshot and later blocks served by another data
// directory, exactly as a new node verifies an untrusted peer.
CommandLineResult checkpointVerifyBootstrap(const CommandLineOptions &options) {
  if (options.sourceDataDirectory.empty() || options.trustedCheckpoint.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "checkpoint verify-bootstrap requires --source-dir and "
        "--trusted-checkpoint HEIGHT:CHECKPOINT_ID.\n");
  }
  const auto anchor =
      node::TrustedCheckpointAnchor::parse(options.trustedCheckpoint);
  if (!anchor) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "--trusted-checkpoint must be HEIGHT:CHECKPOINT_ID (64 hex).\n");
  }
  // The verifying node's own genesis is trusted; the source is not.
  const config::GenesisLookupResult genesis =
      node::RuntimeStartupService::resolveGenesis(
          options.networkName, options.dataDirectory, options.genesisFile);
  if (!genesis.found()) {
    return failure("Cannot resolve the local genesis: " + genesis.reason());
  }
  const config::HistoryParameters parameters =
      config::HistoryParameters::forNetwork(
          genesis.genesis().networkParameters().networkName());
  const node::NodeDataDirectoryConfig source(options.sourceDataDirectory);
  const node::HistoryStore store(source);
  try {
    const auto checkpoint = store.loadCheckpoint(anchor->height);
    const auto snapshot =
        store.loadSnapshotBytes(anchor->height, parameters.maxSnapshotBytes());
    if (!checkpoint || !snapshot) {
      return failure("Source does not serve the anchored checkpoint and "
                     "snapshot.");
    }
    std::vector<node::BootstrapFinalizedBlock> later;
    for (std::uint64_t height = anchor->height + 1;; ++height) {
      const std::filesystem::path path =
          node::FinalizedBlockStore::blockFilePath(source, height);
      std::error_code ec;
      if (!std::filesystem::is_regular_file(path, ec)) {
        break;
      }
      const node::FinalizedBlockArtifact artifact =
          node::FinalizedBlockArtifactCodec::readBlockArtifactFile(path);
      later.push_back({artifact.block(), artifact.finalizedRecord()});
    }
    const node::CheckpointBootstrapResult result =
        node::CheckpointBootstrapVerifier::verify(
            genesis.genesis(), parameters, *anchor, *checkpoint, *snapshot, later,
            options.timestamp);
    if (!result.verified) {
      return failure("Bootstrap rejected (" +
                     node::checkpointVerificationStatusToString(result.status) +
                     "): " + result.reason);
    }
    std::ostringstream out;
    out << "Bootstrap verified from checkpoint " << anchor->height << "\n"
        << "Replayed later blocks: " << result.replayedBlocks << "\n"
        << "Verified tip height: " << result.verifiedHeight << "\n"
        << "Verified tip hash: " << result.verifiedBlockHash << "\n"
        << "Verified state root: " << result.verifiedStateRoot << "\n";
    return CommandLineResult::success(out.str());
  } catch (const std::exception &error) {
    return failure(std::string("Bootstrap rejected: ") + error.what());
  }
}

CommandLineResult storageStatus(const CommandLineOptions &options,
                                const DirectoryContext &context) {
  const node::StorageStatusReport report =
      node::StorageStatusReport::collect(context.directory);
  return CommandLineResult::success(
      (options.outputJson ? report.serializeJson() : report.serializeText()) +
      (options.outputJson ? "\n" : ""));
}

CommandLineResult storageMigrate(const CommandLineOptions &options) {
  const node::StorageMigrationResult result = node::StorageMigration::migrate(
      node::NodeDataDirectoryConfig(options.dataDirectory));
  if (!result.success) {
    return failure("Storage migration failed: " + result.reason);
  }
  std::ostringstream out;
  out << "Storage schema: " << result.fromVersion << " -> " << result.toVersion
      << "\n";
  for (const std::string &action : result.actions) {
    out << "  " << action << "\n";
  }
  if (result.actions.empty()) {
    out << "  " << result.reason << "\n";
  }
  return CommandLineResult::success(out.str());
}

CommandLineResult pruning(const CommandLineOptions &options,
                          const DirectoryContext &context, bool run) {
  std::optional<node::NodeStorageMode> mode = modeFromOptions(options);
  // `pruning status` without --mode reports the configured policy.
  if (!run && options.pruningMode == "archive") {
    try {
      if (const auto configured =
              node::HistoryPruningEngine::loadManifest(context.directory)) {
        mode = configured->mode;
      }
    } catch (const std::exception &error) {
      return failure(std::string("Pruning manifest is corrupt: ") + error.what());
    }
  }
  if (!mode) {
    return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                      "Unknown storage mode.\n");
  }
  node::PruningOptions pruning = pruningOptions(options, *mode);
  const node::PruningRunResult result =
      run && !options.dryRun
          ? node::HistoryPruningEngine::run(context.directory, context.genesis,
                                            pruning, options.timestamp)
          : node::HistoryPruningEngine::plan(context.directory, context.genesis,
                                             pruning);
  if (!result.success()) {
    return failure("Pruning rejected: " + result.reason);
  }
  std::ostringstream out;
  out << "Pruning: " << node::pruningRunStatusToString(result.status) << "\n";
  if (!result.reason.empty()) {
    out << "Message: " << result.reason << "\n";
  }
  if (result.recoveredInterruptedRun) {
    out << "Recovered an interrupted pruning run first.\n";
  }
  out << describePlan(result);
  for (const std::string &target : result.removedTargets) {
    out << (result.status == node::PruningRunStatus::APPLIED ? "  removed: "
                                                             : "  would remove: ")
        << target << "\n";
  }
  if (result.manifest) {
    out << "Manifest: " << result.manifest->serializeJson() << "\n";
  }
  return CommandLineResult::success(out.str());
}

CommandLineResult archiveStatus(const CommandLineOptions &options,
                                const DirectoryContext &context) {
  const node::StorageStatusReport report =
      node::StorageStatusReport::collect(context.directory);
  if (options.outputJson) {
    return CommandLineResult::success(report.serializeJson() + "\n");
  }
  std::ostringstream out;
  out << "Storage mode: " << report.mode << "\n"
      << "Archive segment size: " << context.parameters.archiveSegmentBlocks()
      << " blocks\n"
      << "Sealed segments: " << report.archiveSegmentsSealed << "\n"
      << "Committed segments: " << report.archiveSegments << " ("
      << report.archiveBytes << " bytes of history)\n"
      << "Replication target: " << context.parameters.archiveReplicationTarget()
      << " distinct operators\n"
      << "Archival self-audit: " << report.selfAudit.passed << "/"
      << report.selfAudit.challenges << " passed\n"
      << "Network Proof of Archival: not active (ADR 0014 activation pending; "
         "replication is not yet measured on chain)\n";
  return CommandLineResult::success(out.str());
}

CommandLineResult archiveSegments(const CommandLineOptions &options,
                                  const DirectoryContext &context) {
  const node::HistoryStore store(context.directory);
  const auto commitments =
      store.loadSegmentCommitments(store.contiguousSegmentCommitmentCount());
  if (!commitments) {
    return failure("Archive segment commitments are inconsistent.");
  }
  std::ostringstream out;
  if (options.outputJson) {
    out << "{\"segments\":[";
  }
  for (std::size_t index = 0; index < commitments->size(); ++index) {
    const archive::ArchiveSegmentCommitment &c = (*commitments)[index];
    if (options.outputJson) {
      out << (index == 0 ? "" : ",") << "{\"segment\":" << c.segmentIndex()
          << ",\"firstHeight\":" << c.firstHeight()
          << ",\"lastHeight\":" << c.lastHeight()
          << ",\"bytes\":" << c.totalBytes() << ",\"pieces\":" << c.pieceCount()
          << ",\"pieceRoot\":" << jsonString(c.pieceRoot())
          << ",\"segmentId\":" << jsonString(c.segmentId()) << "}";
    } else {
      out << c.segmentIndex() << " heights " << c.firstHeight() << "-"
          << c.lastHeight() << " bytes=" << c.totalBytes()
          << " pieces=" << c.pieceCount() << " id=" << c.segmentId() << "\n";
    }
  }
  if (options.outputJson) {
    out << "]}\n";
  } else if (commitments->empty()) {
    out << "No sealed archive segments yet.\n";
  }
  return CommandLineResult::success(out.str());
}

// Challenges this node's own copy of a sealed segment with the exact
// Proof-of-Archival protocol: derive, prove from local block files, verify.
CommandLineResult archiveSelfAudit(const CommandLineOptions &options,
                                   const DirectoryContext &context) {
  const config::HistoryParameters &parameters = context.parameters;
  const node::HistoryStore store(context.directory);
  const std::uint64_t committed = store.contiguousSegmentCommitmentCount();
  if (committed == 0) {
    return failure("No sealed archive segment is committed yet.");
  }
  const std::uint64_t segmentIndex =
      options.segmentProvided ? options.segmentIndex : committed - 1;
  const auto commitment = store.loadSegmentCommitment(segmentIndex);
  if (!commitment) {
    return failure("Segment " + std::to_string(segmentIndex) +
                   " has no stored commitment.");
  }
  const std::uint64_t tip = context.manifest.latestBlockHeight();
  const std::uint64_t round = tip / parameters.archiveChallengeIntervalBlocks();
  if (parameters.challengeSeedHeight(round) < commitment->lastHeight()) {
    return failure("Segment is not sealed before the latest challenge round.");
  }
  const archive::ArchivedBlockSource source =
      node::CheckpointService::blockFileSource(context.directory);
  const auto seedBlock = source(parameters.challengeSeedHeight(round));
  if (!seedBlock) {
    return failure("The challenge seed block is not stored locally.");
  }

  node::ArchivalSelfAuditCounters counters;
  try {
    if (const auto stored =
            node::ArchivalSelfAuditCounters::load(context.directory)) {
      counters = *stored;
    }
  } catch (const std::exception &) {
  }
  ++counters.challenges;
  counters.lastSegment = segmentIndex;
  counters.updatedAt = options.timestamp;

  std::ostringstream out;
  bool passed = false;
  try {
    const archive::ArchiveSegmentIndex local = archive::ArchiveSegmentBuilder::build(
        parameters, context.manifest.chainId(), segmentIndex, source);
    if (local.commitment().encode() != commitment->encode()) {
      throw std::runtime_error(
          "local block files no longer match the committed segment");
    }
    // A local audit identity: same protocol as a network provider, no stake.
    const crypto::KeyPair key = crypto::KeyPair::createDeterministicEd25519KeyPair(
        "nodo-archive-self-audit:" + context.genesis.deterministicId());
    archive::ArchiveProviderRegistry registry(context.manifest.chainId());
    archive::ArchiveProviderRegistrationFields fields;
    fields.chainId = context.manifest.chainId();
    fields.operatorId = "local-self-audit";
    fields.bondRawUnits = parameters.archiveMinBondPerSlotRawUnits();
    fields.declaredCapacityBytes = commitment->totalBytes();
    fields.registeredHeight = 1;
    registry.registerProvider(
        archive::ArchiveProviderRegistration::sign(fields, key, options.timestamp),
        parameters);
    const std::uint64_t activation =
        1 + parameters.archiveProviderActivationDelayBlocks();
    const auto activationBlock = source(activation);
    registry.activateAt(activation, activationBlock ? activationBlock->blockHash
                                                    : seedBlock->blockHash);

    const archive::ArchivalChallenge challenge = archive::ArchivalChallenge::derive(
        parameters, key.address().value(), *commitment, round,
        seedBlock->blockHash);
    const archive::ArchivalProof proof = archive::ArchivalProof::build(
        challenge, local, source, challenge.fields().issueHeight, key,
        options.timestamp);
    const archive::ArchivalProofVerdict verdict =
        archive::ArchivalProofVerifier::verify(parameters, challenge, *commitment,
                                               registry, proof,
                                               archive::ArchivalReplayGuard(),
                                               proof.submittedHeight());
    passed = verdict.accepted();
    out << "Segment " << segmentIndex << " (heights " << commitment->firstHeight()
        << "-" << commitment->lastHeight() << ", " << commitment->totalBytes()
        << " bytes, " << commitment->pieceCount() << " pieces)\n"
        << "Challenge round: " << round << "\n"
        << "Sampled pieces: " << challenge.fields().sampleIndices.size() << "\n"
        << "Proof size: " << proof.encode().size() << " bytes\n"
        << "Verdict: " << archive::archivalProofStatusToString(verdict.status)
        << (verdict.reason.empty() ? "" : " (" + verdict.reason + ")") << "\n";
  } catch (const std::exception &error) {
    out << "Self-audit could not produce a proof: " << error.what() << "\n";
  }
  if (passed) {
    ++counters.passed;
  } else {
    ++counters.failed;
  }
  try {
    counters.save(context.directory);
  } catch (const std::exception &) {
  }
  return passed ? CommandLineResult::success(out.str() + "Self-audit PASSED\n")
                : failure(out.str() + "Self-audit FAILED");
}

} // namespace

bool CommandLineInterface::isHistoryCommand(const std::string &command) {
  return command.rfind("checkpoint ", 0) == 0 ||
         command.rfind("storage ", 0) == 0 ||
         command.rfind("pruning ", 0) == 0 || command.rfind("archive ", 0) == 0;
}

CommandLineResult
CommandLineInterface::executeHistoryCommand(const CommandLineOptions &options) {
  const std::string &command = options.command;
  if (command == "checkpoint verify-bootstrap") {
    return checkpointVerifyBootstrap(options);
  }
  if (command == "storage migrate") {
    return storageMigrate(options);
  }
  std::string error;
  const std::optional<DirectoryContext> context = openDirectory(options, error);
  if (!context) {
    return failure(error);
  }
  if (command == "checkpoint status") {
    return checkpointStatus(options, *context);
  }
  if (command == "checkpoint list") {
    return checkpointList(options, *context);
  }
  if (command == "checkpoint show") {
    return checkpointShow(options, *context);
  }
  if (command == "checkpoint verify") {
    return checkpointVerify(options, *context);
  }
  if (command == "checkpoint backfill") {
    return checkpointBackfill(*context);
  }
  if (command == "storage status") {
    return storageStatus(options, *context);
  }
  if (command == "pruning status") {
    return pruning(options, *context, false);
  }
  if (command == "pruning run") {
    return pruning(options, *context, true);
  }
  if (command == "archive status") {
    return archiveStatus(options, *context);
  }
  if (command == "archive segments") {
    return archiveSegments(options, *context);
  }
  if (command == "archive enable") {
    node::PruningOptions archiveOnly;
    archiveOnly.mode = node::NodeStorageMode::ARCHIVE;
    const node::PruningRunResult result = node::HistoryPruningEngine::run(
        context->directory, context->genesis, archiveOnly, options.timestamp);
    return result.success()
               ? CommandLineResult::success(
                     "Archive mode enabled: every finalized block, checkpoint "
                     "and snapshot is kept.\n")
               : failure("Cannot enable archive mode: " + result.reason);
  }
  if (command == "archive self-audit") {
    return archiveSelfAudit(options, *context);
  }
  return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                    "Unknown command: " + command + "\n\n" +
                                        helpText());
}

} // namespace nodo::app
