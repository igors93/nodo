#include "config/HistoryParameters.hpp"

#include "core/ProtocolLimits.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/V1EncodingPrimitives.hpp"
#include "serialization/V1MerkleTree.hpp"
#include "utils/Amount.hpp"
#include "utils/SafeScalar.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::config {

namespace {

constexpr std::int64_t kRawUnitsPerNodo = utils::Amount::UNITS_PER_NODO;

std::vector<ArchiveScarcityTier> developmentTiers() {
  return {{3, 10000}, {2, 15000}, {1, 25000}};
}

// Upper bound on one inclusion proof: one sibling per tree level for the
// largest allowed tree, plus fixed per-sample framing.
std::uint64_t maxSampleProofBytes(std::uint32_t pieceBytes) {
  constexpr std::uint64_t kSampleFramingBytes = 64;
  return static_cast<std::uint64_t>(pieceBytes) +
         serialization::V1MerkleTree::kMaxProofSiblings * 32 +
         kSampleFramingBytes;
}

} // namespace

HistoryParameters::HistoryParameters() : m_values() {}

HistoryParameters::HistoryParameters(HistoryParameterValues values)
    : m_values(std::move(values)) {}

HistoryParameters HistoryParameters::developmentLocal() {
  return HistoryParameters(HistoryParameterValues{
      .networkName = "localnet",
      .checkpointIntervalBlocks = 8,
      .checkpointConfirmationBlocks = 2,
      .weakSubjectivitySeconds = kMaxWeakSubjectivitySeconds,
      .minimumPruningRetentionBlocks = 16,
      .minimumRetainedCheckpointSnapshots = 2,
      .maxSnapshotBytes = 64 * 1024 * 1024,
      .snapshotChunkBytes = 16 * 1024,
      .archiveSegmentBlocks = 4,
      .archivePieceBytes = 512,
      .archiveChallengeIntervalBlocks = 4,
      .archiveChallengeResponseBlocks = 2,
      .archiveChallengeSamples = 4,
      .maxArchivalProofBytes = 1024 * 1024,
      .archiveReplicationTarget = 3,
      .archiveMinProvenReplicasBeforePrune = 0,
      .archiveProviderActivationDelayBlocks = 2,
      .archiveMinBondPerSlotRawUnits = kRawUnitsPerNodo,
      .archiveRewardShareBasisPoints = 1000,
      .archiveMinAvailabilityBasisPoints = 8000,
      .archiveReliabilityFloorBasisPoints = 5000,
      .archiveReliabilityRampEpochs = 4,
      .archiveRemovalFailedEpochs = 3,
      .archiveFraudPenaltyBasisPoints = 1000,
      .archiveScarcityTiers = developmentTiers(),
  });
}

HistoryParameters HistoryParameters::developmentSoak() {
  return HistoryParameters(HistoryParameterValues{
      .networkName = "localnet-soak",
      .checkpointIntervalBlocks = 64,
      .checkpointConfirmationBlocks = 8,
      .weakSubjectivitySeconds = kMaxWeakSubjectivitySeconds,
      .minimumPruningRetentionBlocks = 128,
      .minimumRetainedCheckpointSnapshots = 2,
      .maxSnapshotBytes = 256 * 1024 * 1024,
      .snapshotChunkBytes = 64 * 1024,
      .archiveSegmentBlocks = 16,
      .archivePieceBytes = 4096,
      .archiveChallengeIntervalBlocks = 16,
      .archiveChallengeResponseBlocks = 8,
      .archiveChallengeSamples = 8,
      .maxArchivalProofBytes = 4 * 1024 * 1024,
      .archiveReplicationTarget = 3,
      .archiveMinProvenReplicasBeforePrune = 0,
      .archiveProviderActivationDelayBlocks = 16,
      .archiveMinBondPerSlotRawUnits = kRawUnitsPerNodo,
      .archiveRewardShareBasisPoints = 1000,
      .archiveMinAvailabilityBasisPoints = 8000,
      .archiveReliabilityFloorBasisPoints = 5000,
      .archiveReliabilityRampEpochs = 4,
      .archiveRemovalFailedEpochs = 3,
      .archiveFraudPenaltyBasisPoints = 1000,
      .archiveScarcityTiers = developmentTiers(),
  });
}

HistoryParameters HistoryParameters::testnetCandidate() {
  // One checkpoint and one archive segment per 43200-block validator epoch.
  return HistoryParameters(HistoryParameterValues{
      .networkName = "testnet-candidate",
      .checkpointIntervalBlocks = 43200,
      .checkpointConfirmationBlocks = 600,
      .weakSubjectivitySeconds = kMaxWeakSubjectivitySeconds,
      .minimumPruningRetentionBlocks = 86400,
      .minimumRetainedCheckpointSnapshots = 3,
      .maxSnapshotBytes = 1024 * 1024 * 1024,
      .snapshotChunkBytes = 256 * 1024,
      .archiveSegmentBlocks = 43200,
      .archivePieceBytes = 64 * 1024,
      .archiveChallengeIntervalBlocks = 4320,
      .archiveChallengeResponseBlocks = 720,
      .archiveChallengeSamples = 32,
      .maxArchivalProofBytes = 4 * 1024 * 1024,
      .archiveReplicationTarget = 5,
      .archiveMinProvenReplicasBeforePrune = 3,
      .archiveProviderActivationDelayBlocks = 43200,
      .archiveMinBondPerSlotRawUnits = 100 * kRawUnitsPerNodo,
      .archiveRewardShareBasisPoints = 1000,
      .archiveMinAvailabilityBasisPoints = 9000,
      .archiveReliabilityFloorBasisPoints = 5000,
      .archiveReliabilityRampEpochs = 8,
      .archiveRemovalFailedEpochs = 3,
      .archiveFraudPenaltyBasisPoints = 1000,
      .archiveScarcityTiers =
          {{5, 10000}, {4, 12500}, {3, 15000}, {2, 20000}, {1, 30000}},
  });
}

HistoryParameters HistoryParameters::forNetwork(const std::string &networkName) {
  if (networkName == "localnet") {
    return developmentLocal();
  }
  if (networkName == "localnet-soak") {
    return developmentSoak();
  }
  if (networkName == "testnet-candidate") {
    return testnetCandidate();
  }
  if (networkName == "mainnet") {
    throw std::invalid_argument(
        "mainnet history parameters are not defined until the mainnet "
        "profile is locked.");
  }
  throw std::invalid_argument("Unknown network for history parameters: " +
                              networkName);
}

const HistoryParameterValues &HistoryParameters::values() const {
  return m_values;
}
const std::string &HistoryParameters::networkName() const {
  return m_values.networkName;
}
std::uint64_t HistoryParameters::checkpointIntervalBlocks() const {
  return m_values.checkpointIntervalBlocks;
}
std::uint64_t HistoryParameters::checkpointConfirmationBlocks() const {
  return m_values.checkpointConfirmationBlocks;
}
std::int64_t HistoryParameters::weakSubjectivitySeconds() const {
  return m_values.weakSubjectivitySeconds;
}
std::uint64_t HistoryParameters::minimumPruningRetentionBlocks() const {
  return m_values.minimumPruningRetentionBlocks;
}
std::uint32_t HistoryParameters::minimumRetainedCheckpointSnapshots() const {
  return m_values.minimumRetainedCheckpointSnapshots;
}
std::uint64_t HistoryParameters::maxSnapshotBytes() const {
  return m_values.maxSnapshotBytes;
}
std::uint32_t HistoryParameters::snapshotChunkBytes() const {
  return m_values.snapshotChunkBytes;
}
std::uint64_t HistoryParameters::archiveSegmentBlocks() const {
  return m_values.archiveSegmentBlocks;
}
std::uint32_t HistoryParameters::archivePieceBytes() const {
  return m_values.archivePieceBytes;
}
std::uint64_t HistoryParameters::archiveChallengeIntervalBlocks() const {
  return m_values.archiveChallengeIntervalBlocks;
}
std::uint64_t HistoryParameters::archiveChallengeResponseBlocks() const {
  return m_values.archiveChallengeResponseBlocks;
}
std::uint32_t HistoryParameters::archiveChallengeSamples() const {
  return m_values.archiveChallengeSamples;
}
std::uint64_t HistoryParameters::maxArchivalProofBytes() const {
  return m_values.maxArchivalProofBytes;
}
std::uint32_t HistoryParameters::archiveReplicationTarget() const {
  return m_values.archiveReplicationTarget;
}
std::uint32_t HistoryParameters::archiveMinProvenReplicasBeforePrune() const {
  return m_values.archiveMinProvenReplicasBeforePrune;
}
std::uint64_t HistoryParameters::archiveProviderActivationDelayBlocks() const {
  return m_values.archiveProviderActivationDelayBlocks;
}
std::uint64_t HistoryParameters::archiveMinBondPerSlotRawUnits() const {
  return m_values.archiveMinBondPerSlotRawUnits;
}
std::uint32_t HistoryParameters::archiveRewardShareBasisPoints() const {
  return m_values.archiveRewardShareBasisPoints;
}
std::uint32_t HistoryParameters::archiveMinAvailabilityBasisPoints() const {
  return m_values.archiveMinAvailabilityBasisPoints;
}
std::uint32_t HistoryParameters::archiveReliabilityFloorBasisPoints() const {
  return m_values.archiveReliabilityFloorBasisPoints;
}
std::uint32_t HistoryParameters::archiveReliabilityRampEpochs() const {
  return m_values.archiveReliabilityRampEpochs;
}
std::uint32_t HistoryParameters::archiveRemovalFailedEpochs() const {
  return m_values.archiveRemovalFailedEpochs;
}
std::uint32_t HistoryParameters::archiveFraudPenaltyBasisPoints() const {
  return m_values.archiveFraudPenaltyBasisPoints;
}
const std::vector<ArchiveScarcityTier> &
HistoryParameters::archiveScarcityTiers() const {
  return m_values.archiveScarcityTiers;
}

std::string HistoryParameters::validationError() const {
  const HistoryParameterValues &v = m_values;
  if (!utils::isSafeIdentifier(v.networkName, 64, "_-.")) {
    return "network name is not a safe identifier";
  }
  if (v.checkpointIntervalBlocks == 0 ||
      v.checkpointIntervalBlocks > kMaxCheckpointIntervalBlocks) {
    return "checkpoint interval is out of range";
  }
  if (v.weakSubjectivitySeconds <= 0 ||
      v.weakSubjectivitySeconds > kMaxWeakSubjectivitySeconds) {
    return "weak-subjectivity window must be positive and at most 14 days";
  }
  if (v.minimumPruningRetentionBlocks < v.checkpointIntervalBlocks) {
    return "pruning retention must cover at least one checkpoint interval";
  }
  if (v.minimumRetainedCheckpointSnapshots < kMinRetainedCheckpointSnapshots) {
    return "at least two checkpoint snapshots must be retained";
  }
  if (v.snapshotChunkBytes < kMinSnapshotChunkBytes ||
      v.snapshotChunkBytes > kMaxSnapshotChunkBytes) {
    return "snapshot chunk size is out of range";
  }
  if (v.maxSnapshotBytes < v.snapshotChunkBytes ||
      v.maxSnapshotBytes > kMaxSnapshotBytes ||
      (v.maxSnapshotBytes + v.snapshotChunkBytes - 1) / v.snapshotChunkBytes >
          serialization::V1MerkleTree::kMaxLeaves) {
    return "maximum snapshot size is out of range";
  }
  if (v.archiveSegmentBlocks == 0 ||
      v.checkpointIntervalBlocks % v.archiveSegmentBlocks != 0) {
    return "checkpoint interval must be a multiple of the archive segment size";
  }
  if (v.archivePieceBytes < kMinArchivePieceBytes ||
      v.archivePieceBytes > kMaxArchivePieceBytes) {
    return "archive piece size is out of range";
  }
  {
    const unsigned __int128 worstSegmentBytes =
        static_cast<unsigned __int128>(v.archiveSegmentBlocks) *
        (core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES +
         kArchiveBlockFramingBytes);
    const unsigned __int128 worstPieces =
        (worstSegmentBytes + v.archivePieceBytes - 1) / v.archivePieceBytes;
    if (worstPieces > serialization::V1MerkleTree::kMaxLeaves) {
      return "a full archive segment would exceed the v1 Merkle leaf limit";
    }
  }
  if (v.archiveChallengeIntervalBlocks == 0 ||
      v.archiveChallengeResponseBlocks == 0 ||
      v.archiveChallengeResponseBlocks > v.archiveChallengeIntervalBlocks) {
    return "a challenge must be due before the next round is issued";
  }
  if (v.archiveChallengeSamples == 0 ||
      v.archiveChallengeSamples > kMaxChallengeSamples) {
    return "challenge sample count is out of range";
  }
  if (v.maxArchivalProofBytes > kMaxArchivalProofBytes ||
      v.maxArchivalProofBytes <
          v.archiveChallengeSamples * maxSampleProofBytes(v.archivePieceBytes)) {
    return "archival proof cap cannot hold a worst-case proof";
  }
  if (v.archiveReplicationTarget == 0 ||
      v.archiveReplicationTarget > kMaxReplicationTarget ||
      v.archiveMinProvenReplicasBeforePrune > v.archiveReplicationTarget) {
    return "archive replication target is out of range";
  }
  if (v.archiveProviderActivationDelayBlocks == 0) {
    return "provider activation delay must be positive";
  }
  if (v.archiveMinBondPerSlotRawUnits == 0 ||
      v.archiveMinBondPerSlotRawUnits >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return "minimum archive bond is out of range";
  }
  if (v.archiveRewardShareBasisPoints > kMaxArchiveRewardShareBasisPoints) {
    return "archival reward share exceeds the protocol bound";
  }
  if (v.archiveMinAvailabilityBasisPoints == 0 ||
      v.archiveMinAvailabilityBasisPoints > kBasisPoints ||
      v.archiveReliabilityFloorBasisPoints == 0 ||
      v.archiveReliabilityFloorBasisPoints > kBasisPoints ||
      v.archiveFraudPenaltyBasisPoints > kBasisPoints) {
    return "archival basis-point parameter is out of range";
  }
  if (v.archiveReliabilityRampEpochs == 0 ||
      v.archiveRemovalFailedEpochs == 0) {
    return "reliability ramp and removal epochs must be positive";
  }
  const auto &tiers = v.archiveScarcityTiers;
  if (tiers.empty() || tiers.size() > kMaxScarcityTiers) {
    return "scarcity tier count is out of range";
  }
  if (tiers.front().minProvenReplicas != v.archiveReplicationTarget ||
      tiers.front().multiplierBasisPoints != kBasisPoints ||
      tiers.back().minProvenReplicas != 1) {
    return "scarcity tiers must start at the target with 1.0x and end at one "
           "replica";
  }
  for (std::size_t index = 0; index < tiers.size(); ++index) {
    if (tiers[index].multiplierBasisPoints < kBasisPoints ||
        tiers[index].multiplierBasisPoints > kMaxScarcityMultiplierBasisPoints) {
      return "scarcity multiplier is out of range";
    }
    if (index > 0 &&
        (tiers[index].minProvenReplicas >= tiers[index - 1].minProvenReplicas ||
         tiers[index].multiplierBasisPoints <
             tiers[index - 1].multiplierBasisPoints)) {
      return "scarcity tiers must lower replicas and never lower the "
             "multiplier";
    }
  }
  return "";
}

bool HistoryParameters::isValid() const { return validationError().empty(); }

std::string HistoryParameters::deterministicId() const {
  if (!isValid()) {
    throw std::logic_error("Invalid history parameters have no identity: " +
                           validationError());
  }
  const HistoryParameterValues &v = m_values;
  serialization::CanonicalWriter writer;
  writer.writeString("NODO_HISTORY_PARAMETERS_V1");
  writer.writeString(v.networkName);
  writer.writeUInt64(v.checkpointIntervalBlocks);
  writer.writeUInt32(v.snapshotChunkBytes);
  writer.writeUInt64(v.archiveSegmentBlocks);
  writer.writeUInt32(v.archivePieceBytes);
  writer.writeUInt64(v.archiveChallengeIntervalBlocks);
  writer.writeUInt64(v.archiveChallengeResponseBlocks);
  writer.writeUInt32(v.archiveChallengeSamples);
  writer.writeUInt64(v.maxArchivalProofBytes);
  writer.writeUInt32(v.archiveReplicationTarget);
  writer.writeUInt64(v.archiveProviderActivationDelayBlocks);
  writer.writeUInt64(v.archiveMinBondPerSlotRawUnits);
  writer.writeUInt32(v.archiveRewardShareBasisPoints);
  writer.writeUInt32(v.archiveMinAvailabilityBasisPoints);
  writer.writeUInt32(v.archiveReliabilityFloorBasisPoints);
  writer.writeUInt32(v.archiveReliabilityRampEpochs);
  writer.writeUInt32(v.archiveRemovalFailedEpochs);
  writer.writeUInt32(v.archiveFraudPenaltyBasisPoints);
  writer.writeUInt32(static_cast<std::uint32_t>(v.archiveScarcityTiers.size()));
  for (const ArchiveScarcityTier &tier : v.archiveScarcityTiers) {
    writer.writeUInt32(tier.minProvenReplicas);
    writer.writeUInt32(tier.multiplierBasisPoints);
  }
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1EncodingPrimitives::hash("HISTORY/PARAMETERS",
                                                writer.bytes()));
}

std::string HistoryParameters::serialize() const {
  const HistoryParameterValues &v = m_values;
  std::ostringstream oss;
  oss << "HistoryParameters{network=" << v.networkName
      << ";checkpointIntervalBlocks=" << v.checkpointIntervalBlocks
      << ";checkpointConfirmationBlocks=" << v.checkpointConfirmationBlocks
      << ";weakSubjectivitySeconds=" << v.weakSubjectivitySeconds
      << ";minimumPruningRetentionBlocks=" << v.minimumPruningRetentionBlocks
      << ";minimumRetainedCheckpointSnapshots="
      << v.minimumRetainedCheckpointSnapshots
      << ";archiveSegmentBlocks=" << v.archiveSegmentBlocks
      << ";archivePieceBytes=" << v.archivePieceBytes
      << ";archiveChallengeIntervalBlocks=" << v.archiveChallengeIntervalBlocks
      << ";archiveChallengeSamples=" << v.archiveChallengeSamples
      << ";archiveReplicationTarget=" << v.archiveReplicationTarget
      << ";archiveRewardShareBasisPoints=" << v.archiveRewardShareBasisPoints
      << "}";
  return oss.str();
}

bool HistoryParameters::isCheckpointHeight(std::uint64_t height) const {
  return isValid() && height != 0 &&
         height % m_values.checkpointIntervalBlocks == 0;
}

std::uint64_t
HistoryParameters::checkpointHeightAtOrBelow(std::uint64_t height) const {
  if (!isValid()) {
    throw std::logic_error("Invalid history parameters.");
  }
  return height - height % m_values.checkpointIntervalBlocks;
}

std::uint64_t
HistoryParameters::segmentIndexForHeight(std::uint64_t height) const {
  if (!isValid() || height == 0) {
    throw std::invalid_argument("Archive segments start at height one.");
  }
  return (height - 1) / m_values.archiveSegmentBlocks;
}

std::uint64_t
HistoryParameters::segmentFirstHeight(std::uint64_t segmentIndex) const {
  return segmentLastHeight(segmentIndex) - m_values.archiveSegmentBlocks + 1;
}

std::uint64_t
HistoryParameters::segmentLastHeight(std::uint64_t segmentIndex) const {
  if (!isValid()) {
    throw std::logic_error("Invalid history parameters.");
  }
  const std::uint64_t size = m_values.archiveSegmentBlocks;
  if (segmentIndex >= std::numeric_limits<std::uint64_t>::max() / size) {
    throw std::overflow_error("Archive segment index overflows height.");
  }
  return (segmentIndex + 1) * size;
}

std::uint64_t
HistoryParameters::sealedSegmentCount(std::uint64_t finalizedHeight) const {
  if (!isValid()) {
    throw std::logic_error("Invalid history parameters.");
  }
  return finalizedHeight / m_values.archiveSegmentBlocks;
}

std::uint64_t
HistoryParameters::challengeRoundForHeight(std::uint64_t height) const {
  if (!isValid() || height == 0) {
    throw std::invalid_argument("Challenge rounds start at height one.");
  }
  return (height - 1) / m_values.archiveChallengeIntervalBlocks;
}

std::uint64_t HistoryParameters::challengeSeedHeight(std::uint64_t round) const {
  if (!isValid()) {
    throw std::logic_error("Invalid history parameters.");
  }
  const std::uint64_t interval = m_values.archiveChallengeIntervalBlocks;
  if (round > (std::numeric_limits<std::uint64_t>::max() - 1) / interval) {
    throw std::overflow_error("Challenge round overflows height.");
  }
  return round * interval;
}

std::uint64_t
HistoryParameters::challengeIssueHeight(std::uint64_t round) const {
  return challengeSeedHeight(round) + 1;
}

std::uint64_t
HistoryParameters::challengeDeadlineHeight(std::uint64_t round) const {
  const std::uint64_t issue = challengeIssueHeight(round);
  if (issue > std::numeric_limits<std::uint64_t>::max() -
                  m_values.archiveChallengeResponseBlocks) {
    throw std::overflow_error("Challenge deadline overflows height.");
  }
  return issue + m_values.archiveChallengeResponseBlocks;
}

std::uint32_t HistoryParameters::scarcityMultiplierBasisPoints(
    std::uint32_t provenReplicas) const {
  if (provenReplicas == 0) {
    return 0;
  }
  for (const ArchiveScarcityTier &tier : m_values.archiveScarcityTiers) {
    if (provenReplicas >= tier.minProvenReplicas) {
      return tier.multiplierBasisPoints;
    }
  }
  return 0;
}

} // namespace nodo::config
