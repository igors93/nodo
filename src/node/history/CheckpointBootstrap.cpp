#include "node/history/CheckpointBootstrap.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/AccountabilityWindow.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

CheckpointBootstrapResult rejected(CheckpointVerificationStatus status,
                                   std::string reason) {
  CheckpointBootstrapResult result;
  result.verified = false;
  result.status = status;
  result.reason = std::move(reason);
  return result;
}

CheckpointBootstrapResult rejected(const CheckpointVerificationResult &failure,
                                   const std::string &stage) {
  return rejected(failure.status(), stage + ": " + failure.reason());
}

} // namespace

bool TrustedCheckpointAnchor::isValid() const {
  return height > 0 && archive::isDigestHex(checkpointId);
}

std::string TrustedCheckpointAnchor::serialize() const {
  return std::to_string(height) + ":" + checkpointId;
}

std::optional<TrustedCheckpointAnchor>
TrustedCheckpointAnchor::parse(const std::string &text) {
  const std::size_t separator = text.find(':');
  if (separator == std::string::npos || separator == 0 ||
      separator > 20 || text.find(':', separator + 1) != std::string::npos) {
    return std::nullopt;
  }
  const std::string heightText = text.substr(0, separator);
  if (heightText.find_first_not_of("0123456789") != std::string::npos ||
      (heightText.size() > 1 && heightText.front() == '0')) {
    return std::nullopt;
  }
  TrustedCheckpointAnchor anchor;
  try {
    std::size_t used = 0;
    anchor.height = std::stoull(heightText, &used);
    if (used != heightText.size()) {
      return std::nullopt;
    }
  } catch (const std::exception &) {
    return std::nullopt;
  }
  anchor.checkpointId = text.substr(separator + 1);
  if (!anchor.isValid()) {
    return std::nullopt;
  }
  return anchor;
}

CheckpointBootstrapResult CheckpointBootstrapVerifier::verify(
    const config::GenesisConfig &genesisConfig,
    const config::HistoryParameters &parameters,
    const TrustedCheckpointAnchor &anchor,
    const FinalizedStateCheckpoint &checkpoint,
    const std::vector<unsigned char> &snapshotBytes,
    const std::vector<BootstrapFinalizedBlock> &laterBlocks,
    std::int64_t now) {
  using Status = CheckpointVerificationStatus;

  // 1. The anchor is the only trusted input.
  if (!anchor.isValid()) {
    return rejected(Status::ANCHOR_MISMATCH,
                    "a trusted checkpoint anchor is required");
  }
  if (!checkpoint.isStructurallyValid() ||
      checkpoint.height() != anchor.height ||
      checkpoint.checkpointId() != anchor.checkpointId) {
    return rejected(Status::ANCHOR_MISMATCH,
                    "served checkpoint is not the trusted checkpoint");
  }
  const FinalizedStateCheckpointFields &fields = checkpoint.fields();
  if (now <= 0 ||
      (fields.blockTimestamp > now &&
       fields.blockTimestamp - now >
           AccountabilityWindow::kFutureBlockSkewSeconds)) {
    return rejected(Status::MALFORMED,
                    "checkpoint time is in the future of the local clock");
  }
  if (now >= fields.blockTimestamp &&
      now - fields.blockTimestamp > parameters.weakSubjectivitySeconds()) {
    return rejected(Status::STALE_ANCHOR,
                    "trusted checkpoint is older than the weak-subjectivity "
                    "window; obtain a recent checkpoint out of band");
  }

  // 2. Identity and parameters.
  const CheckpointVerificationResult identity =
      FinalizedStateCheckpointVerifier::verifyIdentity(checkpoint,
                                                       genesisConfig,
                                                       parameters);
  if (!identity.isAccepted()) {
    return rejected(identity, "checkpoint identity");
  }

  // 3. Snapshot: cheap digest check before decoding anything else.
  if (snapshotBytes.empty() ||
      snapshotBytes.size() > parameters.maxSnapshotBytes()) {
    return rejected(Status::SNAPSHOT_DIGEST_MISMATCH,
                    "snapshot size is outside the accepted range");
  }
  try {
    if (FullProtocolStateSnapshot::digestOfEncoding(
            snapshotBytes, parameters.snapshotChunkBytes()) !=
        fields.snapshotDigest) {
      return rejected(Status::SNAPSHOT_DIGEST_MISMATCH,
                      "snapshot bytes do not match the checkpoint digest");
    }
  } catch (const std::exception &error) {
    return rejected(Status::SNAPSHOT_DIGEST_MISMATCH, error.what());
  }
  FullProtocolStateSnapshot snapshot;
  try {
    snapshot = FullProtocolStateSnapshot::decode(snapshotBytes,
                                                 parameters.maxSnapshotBytes());
  } catch (const std::exception &error) {
    return rejected(Status::MALFORMED,
                    std::string("snapshot does not decode: ") + error.what());
  }
  const CheckpointVerificationResult state =
      FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
          snapshot, checkpoint, genesisConfig, parameters);
  if (!state.isAccepted()) {
    return rejected(state, "snapshot");
  }

  // 4. Finality evidence for the checkpoint block.
  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          genesisConfig.networkParameters().networkName());
  const CheckpointVerificationResult certificate =
      FinalizedStateCheckpointVerifier::verifyCertificate(
          checkpoint, snapshot.consensusWindow().setAt(fields.height),
          cryptoContext);
  if (!certificate.isAccepted()) {
    return rejected(certificate, "checkpoint certificate");
  }

  // 5. Replay every later finalized block from the verified state.
  ProtocolReplayState replay;
  try {
    replay = snapshot.toReplayState();
  } catch (const std::exception &error) {
    return rejected(Status::STATE_MISMATCH, error.what());
  }
  const std::uint64_t rawMinimumFee =
      genesisConfig.networkParameters().minimumFeeRawUnits();
  if (rawMinimumFee >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return rejected(Status::MALFORMED, "minimum fee exceeds the Amount range");
  }
  const std::int64_t minimumFee = static_cast<std::int64_t>(rawMinimumFee);

  std::uint64_t expectedHeight = fields.height + 1;
  std::string parentHash = fields.blockHash;
  CheckpointBootstrapResult result;
  for (const BootstrapFinalizedBlock &item : laterBlocks) {
    const core::Block &block = item.block;
    const std::string where = " at height " + std::to_string(expectedHeight);
    if (block.index() != expectedHeight || block.previousHash() != parentHash ||
        !block.isValid(true)) {
      return rejected(Status::BLOCK_MISMATCH,
                      "block does not extend the verified chain" + where);
    }
    try {
      if (!item.finalizedRecord.matchesBlock(block) ||
          !item.finalizedRecord.verify(
              replay.validatorSetHistory.setAt(block.index()),
              cryptoContext.policy(),
              cryptoContext.validatorSignatureProvider())) {
        return rejected(Status::CERTIFICATE_INVALID,
                        "block QC does not verify against its frozen set" +
                            where);
      }
      replay = ProtocolStateTransition::replayBlock(
          genesisConfig, replay, block, minimumFee, block.timestamp());
    } catch (const std::exception &error) {
      return rejected(Status::BLOCK_MISMATCH,
                      std::string("block replay failed") + where + ": " +
                          error.what());
    }
    if (replay.stateRoot != block.stateRoot()) {
      return rejected(Status::STATE_MISMATCH,
                      "replayed state does not match the header state root" +
                          where);
    }
    parentHash = block.hash();
    ++expectedHeight;
    ++result.replayedBlocks;
  }

  result.verified = true;
  result.status = Status::ACCEPTED;
  result.verifiedHeight = expectedHeight - 1;
  result.verifiedBlockHash = parentHash;
  result.verifiedStateRoot = replay.stateRoot;
  result.state = std::move(replay);
  return result;
}

} // namespace nodo::node
