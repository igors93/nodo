#ifndef NODO_ARCHIVE_ARCHIVAL_SCORE_LEDGER_HPP
#define NODO_ARCHIVE_ARCHIVAL_SCORE_LEDGER_HPP

#include "archive/ArchivalChallenge.hpp"
#include "archive/ArchivalProof.hpp"
#include "archive/ArchiveProvider.hpp"
#include "config/HistoryParameters.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::archive {

enum class ChallengeOutcome { PENDING, PASSED, FRAUD, MISSED };

std::string challengeOutcomeToString(ChallengeOutcome outcome);

struct ArchivalSlotTally {
  std::string providerId;
  std::string operatorId;
  std::uint64_t segmentIndex = 0;
  std::uint32_t issued = 0;
  std::uint32_t passed = 0;
  std::uint32_t fraud = 0;
  std::uint32_t missed = 0;

  std::uint32_t availabilityBasisPoints() const;
};

struct ArchivalProviderEpochResult {
  std::string providerId;
  std::string operatorId;
  std::uint32_t issued = 0;
  std::uint32_t passed = 0;
  std::uint32_t availabilityBasisPoints = 0;
  bool failedEpoch = false;
  bool fraud = false;
  std::uint32_t reliabilityBasisPoints = 0;
  bool removed = false;
};

struct ArchivalEpochSummary {
  std::uint64_t epoch = 0;
  std::uint64_t firstHeight = 0;
  std::uint64_t lastHeight = 0;
  std::vector<ArchivalSlotTally> slots;
  std::vector<ArchivalProviderEpochResult> providers;
  std::vector<std::string> faultEvidenceIds;
  std::uint64_t challenges = 0;
  std::uint64_t passed = 0;
  std::uint64_t fraud = 0;
  std::uint64_t missed = 0;

  // Distinct operator labels in this summary that met the availability floor.
  // This alone does not authenticate assignment; use ArchiveReplicationReport
  // with the finalized slot set for a replication decision.
  std::uint32_t provenReplicas(std::uint64_t segmentIndex,
                               std::uint32_t minAvailabilityBasisPoints) const;
  std::string digest() const;
};

/*
 * ArchivalScoreLedger records every derived challenge and its outcome and
 * rolls them up per epoch (ADR 0014):
 *
 *   PASSED   a valid proof arrived within the window;
 *   FRAUD    the provider signed an invalid answer (verifiable evidence);
 *   MISSED   no valid proof by the deadline. Not evidence: it may be
 *            censorship or an outage, so it only lowers score and reward.
 *
 * Availability is passed/issued. Reliability ramps from the floor to 1.0x
 * over consecutive good epochs and drops back to the floor after a failed
 * one. Persistent failure removes a provider; fraud removes it at once and
 * is the only path that can justify a bond penalty. Deterministic: the same
 * finalized challenges and proofs yield the same digest everywhere.
 */
class ArchivalScoreLedger {
public:
  explicit ArchivalScoreLedger(config::HistoryParameters parameters);

  // Returns false for a duplicate or invalid challenge.
  bool issue(const ArchivalChallenge &challenge);

  ArchivalProofVerdict
  submit(const ArchivalProof &proof,
         const std::vector<ArchiveSegmentCommitment> &segments,
         const ArchiveProviderRegistry &registry,
         // Height of the finalized block carrying the proof, not a peer claim.
         std::uint64_t finalizedHeight);

  // Every pending challenge whose deadline is below `height` becomes MISSED.
  std::size_t expire(std::uint64_t height);

  // Rolls up challenges issued within [firstHeight, lastHeight]. Every such
  // challenge must already be resolved (call expire past their deadlines).
  ArchivalEpochSummary closeEpoch(std::uint64_t epoch, std::uint64_t firstHeight,
                                  std::uint64_t lastHeight,
                                  ArchiveProviderRegistry &registry);

  std::uint32_t reliabilityBasisPoints(std::uint32_t successfulEpochStreak) const;

  ChallengeOutcome outcome(const std::string &challengeId) const;
  const std::vector<ArchivalFaultEvidence> &faultEvidence() const;
  std::uint64_t issuedCount() const;
  std::uint64_t passedCount() const;
  std::uint64_t fraudCount() const;
  std::uint64_t missedCount() const;
  std::string digest() const;

private:
  struct Entry {
    ArchivalChallenge challenge;
    ChallengeOutcome outcome = ChallengeOutcome::PENDING;
    std::string proofId;
  };

  struct ClosedEpoch {
    std::uint64_t firstHeight = 0;
    std::uint64_t lastHeight = 0;
    ArchivalEpochSummary summary;
  };

  config::HistoryParameters m_parameters;
  std::map<std::string, Entry> m_entries;
  ArchivalReplayGuard m_guard;
  std::vector<ArchivalFaultEvidence> m_faults;
  std::map<std::uint64_t, ClosedEpoch> m_closedEpochs;
};

struct SegmentReplicationStatus {
  std::uint64_t segmentIndex = 0;
  std::uint64_t totalBytes = 0;
  std::uint32_t assignedReplicas = 0;
  std::uint32_t provenReplicas = 0;
  std::uint32_t scarcityMultiplierBasisPoints = 0;
  bool underReplicated = false;
  bool critical = false;
};

/*
 * Replication monitor. Counts proven distinct operators, never claimed or
 * assigned identities, and estimates the bytes preserved. Costs are kept in
 * byte units (bytes x replicas); prices are a governance decision.
 */
struct ArchiveReplicationReport {
  std::vector<SegmentReplicationStatus> segments;
  std::uint32_t minProvenReplicas = 0;
  // Average proven replicas x 10000, to stay in integers.
  std::uint64_t averageProvenReplicasBasisPoints = 0;
  std::vector<std::uint64_t> underReplicatedSegments;
  std::vector<std::uint64_t> criticalSegments;
  std::uint64_t historyBytes = 0;
  std::uint64_t provenReplicaBytes = 0;
  std::uint64_t targetReplicaBytes = 0;

  static ArchiveReplicationReport
  build(const config::HistoryParameters &parameters,
        const std::vector<ArchiveSegmentCommitment> &segments,
        const std::vector<ArchiveSlot> &slots,
        const ArchivalEpochSummary *summary);

  std::vector<std::uint32_t> provenReplicasBySegment() const;
  std::string serializeJson() const;
};

} // namespace nodo::archive

#endif
