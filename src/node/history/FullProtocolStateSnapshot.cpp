#include "node/history/FullProtocolStateSnapshot.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "node/AccountabilityWindow.hpp"
#include "node/FastSyncSnapshotVerifier.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtocolDomainCodec.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "node/ValidatorSetSchedule.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/V1MerkleTree.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

// Validator sets change at most once per epoch, so a window covering the
// evidence age holds at most ~22 sets; this generous cap bounds decoding.
constexpr std::uint32_t kMaxWindowSets = 4096;

std::vector<unsigned char>
encodeWindow(const core::ValidatorSetHistory &window) {
  serialization::CanonicalWriter writer;
  writer.writeString(FullProtocolStateSnapshot::WINDOW_SCHEMA);
  writer.writeUInt64(window.firstRecordedHeight());
  writer.writeUInt64(window.highestRecordedHeight());
  const auto &changes = window.recordedChanges();
  writer.writeUInt32(static_cast<std::uint32_t>(changes.size()));
  for (const auto &[height, registry] : changes) {
    writer.writeUInt64(height);
    writer.writeString(ValidatorsDomainCodec::encode(registry));
  }
  return writer.bytes();
}

core::ValidatorSetHistory decodeWindow(const std::vector<unsigned char> &bytes,
                                       std::uint64_t maxBytes) {
  serialization::CanonicalReader reader(bytes,
                                        static_cast<std::size_t>(maxBytes));
  if (reader.readString() != FullProtocolStateSnapshot::WINDOW_SCHEMA) {
    throw std::invalid_argument("Unknown validator-set window schema.");
  }
  const std::uint64_t first = reader.readUInt64();
  const std::uint64_t highest = reader.readUInt64();
  const std::uint32_t count = reader.readUInt32();
  if (count == 0 || count > kMaxWindowSets) {
    throw std::invalid_argument("Validator-set window count is out of range.");
  }
  std::map<std::uint64_t, core::ValidatorRegistry> changes;
  std::uint64_t previousHeight = 0;
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::uint64_t height = reader.readUInt64();
    if (index > 0 && height <= previousHeight) {
      throw std::invalid_argument("Validator-set window is not ordered.");
    }
    previousHeight = height;
    changes.emplace(height, ValidatorsDomainCodec::decode(reader.readString()));
  }
  reader.requireFullyConsumed();
  std::optional<core::ValidatorSetHistory> window =
      core::ValidatorSetHistory::restore(first, highest, std::move(changes));
  if (!window) {
    throw std::invalid_argument("Validator-set window is invalid.");
  }
  return *window;
}

std::int64_t checkedMinimumFee(const config::GenesisConfig &genesisConfig) {
  const std::uint64_t raw =
      genesisConfig.networkParameters().minimumFeeRawUnits();
  if (raw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("Minimum fee exceeds the Amount range.");
  }
  return static_cast<std::int64_t>(raw);
}

CheckpointVerificationResult reject(CheckpointVerificationStatus status,
                                    std::string reason) {
  return CheckpointVerificationResult::rejected(status, std::move(reason));
}

} // namespace

FullProtocolStateSnapshot::FullProtocolStateSnapshot()
    : m_state(), m_consensusWindow() {}

FullProtocolStateSnapshot::FullProtocolStateSnapshot(
    FastSyncSnapshot state, core::ValidatorSetHistory consensusWindow)
    : m_state(std::move(state)),
      m_consensusWindow(std::move(consensusWindow)) {}

const FastSyncSnapshot &FullProtocolStateSnapshot::state() const {
  return m_state;
}

const core::ValidatorSetHistory &
FullProtocolStateSnapshot::consensusWindow() const {
  return m_consensusWindow;
}

std::uint64_t FullProtocolStateSnapshot::height() const {
  return m_state.blockHeight();
}

std::uint64_t
FullProtocolStateSnapshot::consensusWindowStart(std::uint64_t height) {
  constexpr std::uint64_t age = AccountabilityWindow::kEvidenceMaxAgeBlocks;
  if (height == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Snapshot height overflows the next height.");
  }
  return height + 1 > age ? height + 1 - age : 1;
}

std::string FullProtocolStateSnapshot::consensusContextDigest() const {
  serialization::CanonicalWriter writer;
  writer.writeUInt64(m_consensusWindow.firstRecordedHeight());
  writer.writeUInt64(m_consensusWindow.highestRecordedHeight());
  const auto &changes = m_consensusWindow.recordedChanges();
  writer.writeUInt32(static_cast<std::uint32_t>(changes.size()));
  for (const auto &[height, registry] : changes) {
    writer.writeUInt64(height);
    // The same per-set commitment quorum certificates carry.
    writer.writeString(registry.validatorSetRoot());
  }
  return archive::hashHex("HISTORY/SET-WINDOW", writer.bytes());
}

std::vector<unsigned char> FullProtocolStateSnapshot::encode() const {
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeString(m_state.serialize());
  writer.writeBytes(encodeWindow(m_consensusWindow));
  return writer.bytes();
}

FullProtocolStateSnapshot
FullProtocolStateSnapshot::decode(const std::vector<unsigned char> &bytes,
                                  std::uint64_t maxBytes) {
  if (bytes.empty() || bytes.size() > maxBytes) {
    throw std::invalid_argument("Snapshot encoding size is out of range.");
  }
  serialization::CanonicalReader reader(bytes,
                                        static_cast<std::size_t>(maxBytes));
  if (reader.readString() != SCHEMA) {
    throw std::invalid_argument("Unknown full protocol snapshot schema.");
  }
  const std::string stateText = reader.readString();
  const std::vector<unsigned char> windowBytes = reader.readBytes();
  reader.requireFullyConsumed();

  FullProtocolStateSnapshot snapshot(FastSyncSnapshot::deserialize(stateText),
                                     decodeWindow(windowBytes, maxBytes));
  if (snapshot.encode() != bytes) {
    throw std::invalid_argument("Snapshot encoding is not canonical.");
  }
  return snapshot;
}

std::string FullProtocolStateSnapshot::digest(std::uint32_t chunkBytes) const {
  return digestOfEncoding(encode(), chunkBytes);
}

std::string FullProtocolStateSnapshot::digestOfEncoding(
    const std::vector<unsigned char> &encoded, std::uint32_t chunkBytes) {
  if (encoded.empty() || chunkBytes == 0) {
    throw std::invalid_argument("Snapshot digest needs bytes and a chunk size.");
  }
  const std::uint64_t chunks = (encoded.size() - 1) / chunkBytes + 1;
  if (chunks > serialization::V1MerkleTree::kMaxLeaves) {
    throw std::length_error("Snapshot has too many chunks.");
  }
  std::vector<serialization::V1MerkleTree::Digest> leaves;
  leaves.reserve(static_cast<std::size_t>(chunks));
  const std::span<const unsigned char> all(encoded);
  for (std::uint64_t index = 0; index < chunks; ++index) {
    const std::size_t offset = static_cast<std::size_t>(index * chunkBytes);
    const std::size_t length =
        std::min<std::size_t>(chunkBytes, encoded.size() - offset);
    leaves.push_back(serialization::V1MerkleTree::leafHash(
        "snapshot-chunk", index, all.subspan(offset, length)));
  }
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1MerkleTree::rootFromLeafHashes("snapshot-chunk",
                                                      leaves));
}

ProtocolReplayState FullProtocolStateSnapshot::toReplayState() const {
  ProtocolReplayState state;
  state.accounts = m_state.accountStateView();
  state.execution = ProtocolDomainCodec::decodeState(m_state.protocolDomains());
  state.validatorSetHistory = m_consensusWindow;
  state.stateRoot = m_state.stateRoot();
  return state;
}

FullProtocolStateSnapshot FullProtocolStateSnapshot::fromReplayState(
    const config::GenesisConfig &genesisConfig, std::uint64_t height,
    const std::string &blockHash, std::int64_t blockTimestamp,
    const ProtocolReplayState &replayState) {
  // createdAt is the block timestamp so every honest node encodes the same
  // bytes and therefore derives the same snapshot digest.
  FastSyncSnapshot state = FastSyncSnapshot::fromReplayState(
      genesisConfig, height, blockHash, replayState, blockTimestamp);
  if (replayState.validatorSetHistory.highestRecordedHeight() != height + 1) {
    throw std::invalid_argument(
        "Replay state has not selected the validator set for the next "
        "height.");
  }
  return FullProtocolStateSnapshot(
      std::move(state),
      replayState.validatorSetHistory.windowFrom(consensusWindowStart(height)));
}

FullProtocolStateSnapshot
FullProtocolStateSnapshot::fromRuntime(const NodeRuntime &runtime) {
  if (!runtime.isValid()) {
    throw std::invalid_argument("Cannot snapshot an invalid runtime.");
  }
  const config::GenesisConfig &genesisConfig = runtime.config().genesisConfig();
  const ProtocolReplayState replay =
      ProtocolStateTransition::replayStateFromRuntime(
          runtime, checkedMinimumFee(genesisConfig));
  const core::Block &tip = runtime.blockchain().latestBlock();
  return fromReplayState(genesisConfig, tip.index(), tip.hash(),
                         tip.timestamp(), replay);
}

CheckpointVerificationResult
FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
    const FullProtocolStateSnapshot &snapshot,
    const FinalizedStateCheckpoint &checkpoint,
    const config::GenesisConfig &genesisConfig,
    const config::HistoryParameters &parameters) {
  using Status = CheckpointVerificationStatus;
  if (!checkpoint.isStructurallyValid() || !parameters.isValid()) {
    return reject(Status::MALFORMED, "checkpoint or parameters are invalid");
  }
  const FinalizedStateCheckpointFields &cp = checkpoint.fields();
  const FastSyncSnapshot &state = snapshot.state();

  const FastSyncSnapshotVerificationResult identity =
      FastSyncSnapshotVerifier::verifyForGenesis(state, genesisConfig);
  if (!identity.accepted() || state.genesisConfigId() != cp.genesisConfigId ||
      state.chainId() != cp.chainId || state.networkName() != cp.networkName) {
    return reject(Status::IDENTITY_MISMATCH,
                  "snapshot identity does not match genesis and checkpoint: " +
                      identity.reason());
  }
  if (state.blockHeight() != cp.height || state.blockHash() != cp.blockHash ||
      state.createdAt() != cp.blockTimestamp) {
    return reject(Status::BLOCK_MISMATCH,
                  "snapshot is not for the checkpoint block");
  }
  if (state.stateRoot() != cp.stateRoot ||
      state.accountRoot() != cp.accountsRoot ||
      !state.verifiesProtocolStateRoot()) {
    return reject(Status::STATE_MISMATCH,
                  "snapshot state does not reproduce the checkpoint state "
                  "root");
  }

  const core::ValidatorSetHistory &window = snapshot.consensusWindow();
  const std::uint64_t height = cp.height;
  try {
    if (!window.isValid() ||
        !window.changesOnlyAtBoundaries(NODO_VALIDATOR_EPOCH_BLOCKS) ||
        window.firstRecordedHeight() !=
            FullProtocolStateSnapshot::consensusWindowStart(height) ||
        window.highestRecordedHeight() != height + 1) {
      return reject(Status::CONSENSUS_CONTEXT_MISMATCH,
                    "validator-set window does not cover the evidence age");
    }
    const core::ValidatorRegistry &atHeight = window.setAt(height);
    const core::ValidatorRegistry &next = window.setAt(height + 1);
    if (atHeight.validatorSetRoot() != cp.validatorSetRoot ||
        atHeight.validatorSetRoot() !=
            checkpoint.quorumCertificate().validatorSetRoot() ||
        next.validatorSetRoot() != cp.nextValidatorSetRoot ||
        snapshot.consensusContextDigest() != cp.consensusContextDigest) {
      return reject(Status::CONSENSUS_CONTEXT_MISMATCH,
                    "validator-set window does not match the checkpoint");
    }
    // The next set is not free data: it is the deterministic projection of
    // the committed validators domain.
    const ProtocolExecutionState execution =
        ProtocolDomainCodec::decodeState(state.protocolDomains());
    const core::ValidatorRegistry expectedNext =
        ValidatorSetSchedule::isBoundary(height)
            ? ValidatorSetSchedule::project(
                  atHeight, execution.validators,
                  height / NODO_VALIDATOR_EPOCH_BLOCKS + 1)
            : atHeight;
    if (expectedNext.serialize() != next.serialize()) {
      return reject(Status::CONSENSUS_CONTEXT_MISMATCH,
                    "next validator set is not the deterministic projection");
    }
  } catch (const std::exception &error) {
    return reject(Status::CONSENSUS_CONTEXT_MISMATCH,
                  std::string("validator-set window is unusable: ") +
                      error.what());
  }

  try {
    const std::vector<unsigned char> encoded = snapshot.encode();
    if (encoded.size() > parameters.maxSnapshotBytes() ||
        FullProtocolStateSnapshot::digestOfEncoding(
            encoded, parameters.snapshotChunkBytes()) != cp.snapshotDigest) {
      return reject(Status::SNAPSHOT_DIGEST_MISMATCH,
                    "snapshot digest does not match the checkpoint");
    }
  } catch (const std::exception &error) {
    return reject(Status::SNAPSHOT_DIGEST_MISMATCH, error.what());
  }
  return CheckpointVerificationResult::accepted();
}

} // namespace nodo::node
