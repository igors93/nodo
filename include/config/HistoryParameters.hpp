#ifndef NODO_CONFIG_HISTORY_PARAMETERS_HPP
#define NODO_CONFIG_HISTORY_PARAMETERS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::config {

// Segments whose proven replication is at least minProvenReplicas earn
// multiplierBasisPoints (10000 = 1.0x). See ADR 0014.
struct ArchiveScarcityTier {
  std::uint32_t minProvenReplicas = 0;
  std::uint32_t multiplierBasisPoints = 0;
};

// Named fields prevent silent swaps between adjacent history parameters.
struct HistoryParameterValues {
  std::string networkName;

  // Finalized state checkpoints.
  std::uint64_t checkpointIntervalBlocks = 0;
  std::uint64_t checkpointConfirmationBlocks = 0;
  std::int64_t weakSubjectivitySeconds = 0;

  // Normal-node retention floors. Operators may keep more, never less.
  std::uint64_t minimumPruningRetentionBlocks = 0;
  std::uint32_t minimumRetainedCheckpointSnapshots = 0;

  // Snapshot transfer.
  std::uint64_t maxSnapshotBytes = 0;
  std::uint32_t snapshotChunkBytes = 0;

  // Archive segmentation and Proof of Archival.
  std::uint64_t archiveSegmentBlocks = 0;
  std::uint32_t archivePieceBytes = 0;
  std::uint64_t archiveChallengeIntervalBlocks = 0;
  std::uint64_t archiveChallengeResponseBlocks = 0;
  std::uint32_t archiveChallengeSamples = 0;
  std::uint64_t maxArchivalProofBytes = 0;
  std::uint32_t archiveReplicationTarget = 0;
  std::uint32_t archiveMinProvenReplicasBeforePrune = 0;
  std::uint64_t archiveProviderActivationDelayBlocks = 0;
  std::uint64_t archiveMinBondPerSlotRawUnits = 0;

  // Archival economics. Inactive until ADR 0014 is activated by a protocol
  // upgrade; the reference schedule still uses them for shadow accounting.
  std::uint32_t archiveRewardShareBasisPoints = 0;
  std::uint32_t archiveMinAvailabilityBasisPoints = 0;
  std::uint32_t archiveReliabilityFloorBasisPoints = 0;
  std::uint32_t archiveReliabilityRampEpochs = 0;
  std::uint32_t archiveRemovalFailedEpochs = 0;
  std::uint32_t archiveFraudPenaltyBasisPoints = 0;
  std::vector<ArchiveScarcityTier> archiveScarcityTiers;
};

/*
 * HistoryParameters fixes how a network bounds per-node storage: checkpoint
 * cadence, retention floors, archive segmentation, Proof-of-Archival
 * challenges and the archival reward share (ADR 0014).
 *
 * Security principle:
 * deterministicId() commits only the network-wide fields, so two nodes that
 * would build different checkpoints, segments or challenges disagree on the
 * id instead of silently diverging. Node-local safety margins (confirmation
 * depth, retention floors, weak-subjectivity window, snapshot caps) are
 * excluded because operators may tighten them without forking.
 */
class HistoryParameters {
public:
  static constexpr std::uint32_t kBasisPoints = 10000;
  static constexpr std::int64_t kMaxWeakSubjectivitySeconds =
      14 * 24 * 60 * 60; // ADR 0004
  static constexpr std::uint64_t kMaxCheckpointIntervalBlocks = 10'000'000;
  static constexpr std::uint32_t kMinRetainedCheckpointSnapshots = 2;
  static constexpr std::uint32_t kMinSnapshotChunkBytes = 4096;
  static constexpr std::uint32_t kMaxSnapshotChunkBytes = 256 * 1024;
  static constexpr std::uint64_t kMaxSnapshotBytes =
      std::uint64_t{4} * 1024 * 1024 * 1024;
  static constexpr std::uint32_t kMinArchivePieceBytes = 256;
  static constexpr std::uint32_t kMaxArchivePieceBytes = 1024 * 1024;
  static constexpr std::uint32_t kMaxChallengeSamples = 64;
  static constexpr std::uint32_t kMaxReplicationTarget = 32;
  static constexpr std::uint32_t kMaxArchiveRewardShareBasisPoints = 3000;
  static constexpr std::uint32_t kMaxScarcityMultiplierBasisPoints = 50000;
  static constexpr std::size_t kMaxScarcityTiers = 8;
  // Archival proofs travel in one network payload (ProtocolLimits).
  static constexpr std::uint64_t kMaxArchivalProofBytes = 4 * 1024 * 1024;
  // Framing added to each block in the archived segment stream.
  static constexpr std::uint64_t kArchiveBlockFramingBytes = 12;

  HistoryParameters();
  explicit HistoryParameters(HistoryParameterValues values);

  static HistoryParameters developmentLocal();
  static HistoryParameters developmentSoak();
  static HistoryParameters testnetCandidate();

  // Throws std::invalid_argument for unknown networks and for mainnet, which
  // has no profile until Phase 10 locks its parameters.
  static HistoryParameters forNetwork(const std::string &networkName);

  const HistoryParameterValues &values() const;
  const std::string &networkName() const;
  std::uint64_t checkpointIntervalBlocks() const;
  std::uint64_t checkpointConfirmationBlocks() const;
  std::int64_t weakSubjectivitySeconds() const;
  std::uint64_t minimumPruningRetentionBlocks() const;
  std::uint32_t minimumRetainedCheckpointSnapshots() const;
  std::uint64_t maxSnapshotBytes() const;
  std::uint32_t snapshotChunkBytes() const;
  std::uint64_t archiveSegmentBlocks() const;
  std::uint32_t archivePieceBytes() const;
  std::uint64_t archiveChallengeIntervalBlocks() const;
  std::uint64_t archiveChallengeResponseBlocks() const;
  std::uint32_t archiveChallengeSamples() const;
  std::uint64_t maxArchivalProofBytes() const;
  std::uint32_t archiveReplicationTarget() const;
  std::uint32_t archiveMinProvenReplicasBeforePrune() const;
  std::uint64_t archiveProviderActivationDelayBlocks() const;
  std::uint64_t archiveMinBondPerSlotRawUnits() const;
  std::uint32_t archiveRewardShareBasisPoints() const;
  std::uint32_t archiveMinAvailabilityBasisPoints() const;
  std::uint32_t archiveReliabilityFloorBasisPoints() const;
  std::uint32_t archiveReliabilityRampEpochs() const;
  std::uint32_t archiveRemovalFailedEpochs() const;
  std::uint32_t archiveFraudPenaltyBasisPoints() const;
  const std::vector<ArchiveScarcityTier> &archiveScarcityTiers() const;

  bool isValid() const;
  // Empty when valid; otherwise the first violated rule.
  std::string validationError() const;

  std::string deterministicId() const;
  std::string serialize() const;

  // Checkpoints are cut at every positive multiple of the interval.
  bool isCheckpointHeight(std::uint64_t height) const;
  std::uint64_t checkpointHeightAtOrBelow(std::uint64_t height) const;

  // Segment k holds heights [k*S+1, (k+1)*S]; genesis is never archived as a
  // segment because the genesis document is retained permanently.
  std::uint64_t segmentIndexForHeight(std::uint64_t height) const;
  std::uint64_t segmentFirstHeight(std::uint64_t segmentIndex) const;
  std::uint64_t segmentLastHeight(std::uint64_t segmentIndex) const;
  std::uint64_t sealedSegmentCount(std::uint64_t finalizedHeight) const;

  // Challenge round r is issued at height r*I+1 and seeded by height r*I.
  std::uint64_t challengeRoundForHeight(std::uint64_t height) const;
  std::uint64_t challengeIssueHeight(std::uint64_t round) const;
  std::uint64_t challengeSeedHeight(std::uint64_t round) const;
  std::uint64_t challengeDeadlineHeight(std::uint64_t round) const;

  std::uint32_t scarcityMultiplierBasisPoints(
      std::uint32_t provenReplicas) const;

private:
  HistoryParameterValues m_values;
};

} // namespace nodo::config

#endif
