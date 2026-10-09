#include "archive/ArchivalScoreLedger.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/JsonText.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::archive {

namespace {

constexpr std::uint32_t kBasisPoints = config::HistoryParameters::kBasisPoints;

std::uint32_t ratioBasisPoints(std::uint32_t numerator,
                               std::uint32_t denominator) {
  if (denominator == 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(numerator) * kBasisPoints) / denominator);
}

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right) {
  if (left > std::numeric_limits<std::uint64_t>::max() - right) {
    throw std::overflow_error("Archive byte accounting overflow.");
  }
  return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right) {
  if (right != 0 && left > std::numeric_limits<std::uint64_t>::max() / right) {
    throw std::overflow_error("Archive byte accounting overflow.");
  }
  return left * right;
}

} // namespace

std::string challengeOutcomeToString(ChallengeOutcome outcome) {
  switch (outcome) {
  case ChallengeOutcome::PENDING:
    return "PENDING";
  case ChallengeOutcome::PASSED:
    return "PASSED";
  case ChallengeOutcome::FRAUD:
    return "FRAUD";
  case ChallengeOutcome::MISSED:
    return "MISSED";
  }
  return "PENDING";
}

std::uint32_t ArchivalSlotTally::availabilityBasisPoints() const {
  return ratioBasisPoints(passed, issued);
}

std::uint32_t ArchivalEpochSummary::provenReplicas(
    std::uint64_t segmentIndex, std::uint32_t minAvailabilityBasisPoints) const {
  std::set<std::string> operators;
  for (const ArchivalSlotTally &slot : slots) {
    if (slot.segmentIndex == segmentIndex && slot.fraud == 0 &&
        slot.issued > 0 &&
        slot.availabilityBasisPoints() >= minAvailabilityBasisPoints) {
      operators.insert(slot.operatorId);
    }
  }
  return static_cast<std::uint32_t>(operators.size());
}

std::string ArchivalEpochSummary::digest() const {
  serialization::CanonicalWriter writer;
  writer.writeUInt64(epoch);
  writer.writeUInt64(firstHeight);
  writer.writeUInt64(lastHeight);
  writer.writeUInt32(static_cast<std::uint32_t>(slots.size()));
  for (const ArchivalSlotTally &slot : slots) {
    writer.writeString(slot.providerId);
    writer.writeString(slot.operatorId);
    writer.writeUInt64(slot.segmentIndex);
    writer.writeUInt32(slot.issued);
    writer.writeUInt32(slot.passed);
    writer.writeUInt32(slot.fraud);
    writer.writeUInt32(slot.missed);
  }
  writer.writeUInt32(static_cast<std::uint32_t>(providers.size()));
  for (const ArchivalProviderEpochResult &provider : providers) {
    writer.writeString(provider.providerId);
    writer.writeString(provider.operatorId);
    writer.writeUInt32(provider.issued);
    writer.writeUInt32(provider.passed);
    writer.writeUInt32(provider.availabilityBasisPoints);
    writer.writeBool(provider.failedEpoch);
    writer.writeBool(provider.fraud);
    writer.writeUInt32(provider.reliabilityBasisPoints);
    writer.writeBool(provider.removed);
  }
  writer.writeUInt32(static_cast<std::uint32_t>(faultEvidenceIds.size()));
  for (const std::string &id : faultEvidenceIds) {
    writer.writeString(id);
  }
  writer.writeUInt64(challenges);
  writer.writeUInt64(passed);
  writer.writeUInt64(fraud);
  writer.writeUInt64(missed);
  return hashHex("ARCHIVE/EPOCH-SUMMARY", writer.bytes());
}

ArchivalScoreLedger::ArchivalScoreLedger(config::HistoryParameters parameters)
    : m_parameters(std::move(parameters)), m_entries(), m_guard(), m_faults() {
  if (!m_parameters.isValid()) {
    throw std::invalid_argument("Archival ledger needs valid parameters.");
  }
}

bool ArchivalScoreLedger::issue(const ArchivalChallenge &challenge) {
  if (!challenge.isStructurallyValid()) {
    return false;
  }
  for (const auto &[epoch, closed] : m_closedEpochs) {
    (void)epoch;
    if (challenge.fields().issueHeight >= closed.firstHeight &&
        challenge.fields().issueHeight <= closed.lastHeight) {
      return false;
    }
  }
  return m_entries
      .emplace(challenge.challengeId(),
               Entry{challenge, ChallengeOutcome::PENDING, ""})
      .second;
}

ArchivalProofVerdict ArchivalScoreLedger::submit(
    const ArchivalProof &proof,
    const std::vector<ArchiveSegmentCommitment> &segments,
    const ArchiveProviderRegistry &registry,
    std::uint64_t finalizedHeight) {
  const auto found = m_entries.find(proof.challengeId());
  if (found == m_entries.end()) {
    ArchivalProofVerdict unknown;
    unknown.status = ArchivalProofStatus::WRONG_CHALLENGE;
    unknown.reason = "no such challenge was issued";
    return unknown;
  }
  Entry &entry = found->second;
  if (entry.outcome == ChallengeOutcome::MISSED) {
    ArchivalProofVerdict late;
    late.status = ArchivalProofStatus::EXPIRED;
    late.reason = "the challenge already expired";
    return late;
  }
  const std::uint64_t segmentIndex = entry.challenge.fields().segmentIndex;
  if (segmentIndex >= segments.size()) {
    ArchivalProofVerdict unknown;
    unknown.status = ArchivalProofStatus::WRONG_CHALLENGE;
    unknown.reason = "segment commitment is unknown";
    return unknown;
  }
  const ArchivalProofVerdict verdict = ArchivalProofVerifier::verify(
      m_parameters, entry.challenge, segments[segmentIndex], registry, proof,
      m_guard, finalizedHeight);
  if (verdict.accepted()) {
    entry.outcome = ChallengeOutcome::PASSED;
    entry.proofId = verdict.proofId;
    m_guard.record(proof.challengeId(), verdict.proofId);
  } else if (verdict.isFraud() && entry.outcome == ChallengeOutcome::PENDING) {
    entry.outcome = ChallengeOutcome::FRAUD;
    entry.proofId = verdict.proofId;
    m_guard.record(proof.challengeId(), verdict.proofId);
    m_faults.emplace_back(entry.challenge, proof, finalizedHeight);
  }
  // Any other rejection (bad signature, wrong challenge, late, duplicate)
  // changes nothing: an unauthenticated or replayed message is not a fact
  // about the provider.
  return verdict;
}

std::size_t ArchivalScoreLedger::expire(std::uint64_t height) {
  std::size_t expired = 0;
  for (auto &[id, entry] : m_entries) {
    (void)id;
    if (entry.outcome == ChallengeOutcome::PENDING &&
        entry.challenge.fields().deadlineHeight < height) {
      entry.outcome = ChallengeOutcome::MISSED;
      ++expired;
    }
  }
  return expired;
}

std::uint32_t ArchivalScoreLedger::reliabilityBasisPoints(
    std::uint32_t successfulEpochStreak) const {
  const std::uint32_t floor = m_parameters.archiveReliabilityFloorBasisPoints();
  const std::uint32_t ramp = m_parameters.archiveReliabilityRampEpochs();
  const std::uint32_t streak = std::min(successfulEpochStreak, ramp);
  return floor + static_cast<std::uint32_t>(
                     (static_cast<std::uint64_t>(kBasisPoints - floor) * streak) /
                     ramp);
}

ArchivalEpochSummary ArchivalScoreLedger::closeEpoch(
    std::uint64_t epoch, std::uint64_t firstHeight, std::uint64_t lastHeight,
    ArchiveProviderRegistry &registry) {
  if (firstHeight == 0 || lastHeight < firstHeight) {
    throw std::invalid_argument("Archival epoch range is invalid.");
  }
  if (const auto existing = m_closedEpochs.find(epoch);
      existing != m_closedEpochs.end()) {
    if (existing->second.firstHeight != firstHeight ||
        existing->second.lastHeight != lastHeight) {
      throw std::invalid_argument("Archival epoch was closed for another range.");
    }
    return existing->second.summary;
  }
  for (const auto &[closedEpoch, closed] : m_closedEpochs) {
    (void)closedEpoch;
    if (firstHeight <= closed.lastHeight && lastHeight >= closed.firstHeight) {
      throw std::invalid_argument("Archival epoch overlaps a closed range.");
    }
  }
  ArchivalEpochSummary summary;
  summary.epoch = epoch;
  summary.firstHeight = firstHeight;
  summary.lastHeight = lastHeight;

  std::map<std::pair<std::string, std::uint64_t>, ArchivalSlotTally> slots;
  for (const auto &[id, entry] : m_entries) {
    (void)id;
    const ArchivalChallengeFields &fields = entry.challenge.fields();
    if (fields.issueHeight < firstHeight || fields.issueHeight > lastHeight) {
      continue;
    }
    if (entry.outcome == ChallengeOutcome::PENDING) {
      throw std::logic_error("Archival epoch has unresolved challenges.");
    }
    ArchivalSlotTally &tally = slots[{fields.providerId, fields.segmentIndex}];
    tally.providerId = fields.providerId;
    tally.segmentIndex = fields.segmentIndex;
    if (const ArchiveProviderRecord *record = registry.find(fields.providerId)) {
      tally.operatorId = record->registration.fields().operatorId;
    }
    ++tally.issued;
    ++summary.challenges;
    switch (entry.outcome) {
    case ChallengeOutcome::PASSED:
      ++tally.passed;
      ++summary.passed;
      break;
    case ChallengeOutcome::FRAUD:
      ++tally.fraud;
      ++summary.fraud;
      break;
    case ChallengeOutcome::MISSED:
      ++tally.missed;
      ++summary.missed;
      break;
    case ChallengeOutcome::PENDING:
      break;
    }
  }

  std::map<std::string, ArchivalProviderEpochResult> providers;
  for (auto &[key, tally] : slots) {
    (void)key;
    ArchivalProviderEpochResult &result = providers[tally.providerId];
    result.providerId = tally.providerId;
    result.operatorId = tally.operatorId;
    result.issued += tally.issued;
    result.passed += tally.passed;
    result.fraud = result.fraud || tally.fraud > 0;
    summary.slots.push_back(tally);
  }
  for (const ArchivalFaultEvidence &fault : m_faults) {
    const std::uint64_t issued = fault.challenge().fields().issueHeight;
    if (issued >= firstHeight && issued <= lastHeight) {
      summary.faultEvidenceIds.push_back(fault.evidenceId());
    }
  }
  std::sort(summary.faultEvidenceIds.begin(), summary.faultEvidenceIds.end());

  for (auto &[providerId, result] : providers) {
    result.availabilityBasisPoints = ratioBasisPoints(result.passed, result.issued);
    result.failedEpoch =
        result.fraud ||
        result.availabilityBasisPoints <
            m_parameters.archiveMinAvailabilityBasisPoints();
    ArchiveProviderRecord *record = registry.find(providerId);
    if (record != nullptr && record->status == ArchiveProviderStatus::ACTIVE) {
      if (result.failedEpoch) {
        record->successfulEpochStreak = 0;
        ++record->consecutiveFailedEpochs;
      } else {
        ++record->successfulEpochStreak;
        record->consecutiveFailedEpochs = 0;
      }
      if (result.fraud) {
        result.removed = registry.remove(providerId, lastHeight,
                                         "signed an invalid archival proof");
      } else if (record->consecutiveFailedEpochs >=
                 m_parameters.archiveRemovalFailedEpochs()) {
        result.removed = registry.remove(providerId, lastHeight,
                                         "persistently failed archival "
                                         "challenges");
      }
      result.reliabilityBasisPoints =
          reliabilityBasisPoints(record->successfulEpochStreak);
    }
    summary.providers.push_back(result);
  }
  m_closedEpochs.emplace(epoch, ClosedEpoch{firstHeight, lastHeight, summary});
  return summary;
}

ChallengeOutcome
ArchivalScoreLedger::outcome(const std::string &challengeId) const {
  const auto found = m_entries.find(challengeId);
  return found == m_entries.end() ? ChallengeOutcome::PENDING
                                  : found->second.outcome;
}

const std::vector<ArchivalFaultEvidence> &
ArchivalScoreLedger::faultEvidence() const {
  return m_faults;
}

std::uint64_t ArchivalScoreLedger::issuedCount() const {
  return m_entries.size();
}

std::uint64_t ArchivalScoreLedger::passedCount() const {
  return static_cast<std::uint64_t>(std::count_if(
      m_entries.begin(), m_entries.end(), [](const auto &entry) {
        return entry.second.outcome == ChallengeOutcome::PASSED;
      }));
}

std::uint64_t ArchivalScoreLedger::fraudCount() const {
  return static_cast<std::uint64_t>(std::count_if(
      m_entries.begin(), m_entries.end(), [](const auto &entry) {
        return entry.second.outcome == ChallengeOutcome::FRAUD;
      }));
}

std::uint64_t ArchivalScoreLedger::missedCount() const {
  return static_cast<std::uint64_t>(std::count_if(
      m_entries.begin(), m_entries.end(), [](const auto &entry) {
        return entry.second.outcome == ChallengeOutcome::MISSED;
      }));
}

std::string ArchivalScoreLedger::digest() const {
  serialization::CanonicalWriter writer;
  writer.writeString(m_parameters.deterministicId());
  writer.writeUInt32(static_cast<std::uint32_t>(m_entries.size()));
  for (const auto &[id, entry] : m_entries) {
    writer.writeString(id);
    writer.writeString(challengeOutcomeToString(entry.outcome));
    writer.writeString(entry.proofId);
  }
  writer.writeUInt32(static_cast<std::uint32_t>(m_closedEpochs.size()));
  for (const auto &[epoch, closed] : m_closedEpochs) {
    writer.writeUInt64(epoch);
    writer.writeUInt64(closed.firstHeight);
    writer.writeUInt64(closed.lastHeight);
    writer.writeString(closed.summary.digest());
  }
  return hashHex("ARCHIVE/LEDGER", writer.bytes());
}

ArchiveReplicationReport ArchiveReplicationReport::build(
    const config::HistoryParameters &parameters,
    const std::vector<ArchiveSegmentCommitment> &segments,
    const std::vector<ArchiveSlot> &slots,
    const ArchivalEpochSummary *summary) {
  if (!parameters.isValid() || !ArchiveIndex::isContiguous(segments)) {
    throw std::invalid_argument("Replication report needs sealed segments.");
  }
  ArchiveReplicationReport report;
  std::map<std::uint64_t, std::uint32_t> assigned;
  for (const ArchiveSlot &slot : slots) {
    ++assigned[slot.segmentIndex];
  }
  const std::uint32_t target = parameters.archiveReplicationTarget();
  std::uint64_t provenSum = 0;
  report.minProvenReplicas = segments.empty()
                                 ? 0
                                 : std::numeric_limits<std::uint32_t>::max();
  for (const ArchiveSegmentCommitment &segment : segments) {
    SegmentReplicationStatus status;
    status.segmentIndex = segment.segmentIndex();
    status.totalBytes = segment.totalBytes();
    status.assignedReplicas = assigned[segment.segmentIndex()];
    status.provenReplicas =
        summary == nullptr
            ? 0
            : summary->provenReplicas(segment.segmentIndex(),
                                      parameters.archiveMinAvailabilityBasisPoints());
    status.scarcityMultiplierBasisPoints =
        parameters.scarcityMultiplierBasisPoints(status.provenReplicas);
    status.underReplicated = status.provenReplicas < target;
    status.critical = status.provenReplicas <=
                      std::max<std::uint32_t>(
                          1, parameters.archiveMinProvenReplicasBeforePrune());
    if (status.underReplicated) {
      report.underReplicatedSegments.push_back(status.segmentIndex);
    }
    if (status.critical) {
      report.criticalSegments.push_back(status.segmentIndex);
    }
    report.minProvenReplicas =
        std::min(report.minProvenReplicas, status.provenReplicas);
    provenSum += status.provenReplicas;
    report.historyBytes = checkedAdd(report.historyBytes, segment.totalBytes());
    report.provenReplicaBytes = checkedAdd(
        report.provenReplicaBytes,
        checkedMultiply(segment.totalBytes(), status.provenReplicas));
    report.targetReplicaBytes = checkedAdd(
        report.targetReplicaBytes, checkedMultiply(segment.totalBytes(), target));
    report.segments.push_back(status);
  }
  report.averageProvenReplicasBasisPoints =
      segments.empty() ? 0 : (provenSum * kBasisPoints) / segments.size();
  return report;
}

std::vector<std::uint32_t>
ArchiveReplicationReport::provenReplicasBySegment() const {
  std::vector<std::uint32_t> proven;
  proven.reserve(segments.size());
  for (const SegmentReplicationStatus &status : segments) {
    proven.push_back(status.provenReplicas);
  }
  return proven;
}

std::string ArchiveReplicationReport::serializeJson() const {
  std::ostringstream oss;
  oss << "{\"segmentCount\":" << segments.size()
      << ",\"minProvenReplicas\":" << minProvenReplicas
      << ",\"averageProvenReplicasBasisPoints\":"
      << averageProvenReplicasBasisPoints
      << ",\"historyBytes\":" << historyBytes
      << ",\"provenReplicaBytes\":" << provenReplicaBytes
      << ",\"targetReplicaBytes\":" << targetReplicaBytes
      << ",\"underReplicatedSegments\":[";
  for (std::size_t index = 0; index < underReplicatedSegments.size(); ++index) {
    oss << (index == 0 ? "" : ",") << underReplicatedSegments[index];
  }
  oss << "],\"criticalSegments\":[";
  for (std::size_t index = 0; index < criticalSegments.size(); ++index) {
    oss << (index == 0 ? "" : ",") << criticalSegments[index];
  }
  oss << "],\"segments\":[";
  for (std::size_t index = 0; index < segments.size(); ++index) {
    const SegmentReplicationStatus &status = segments[index];
    oss << (index == 0 ? "" : ",") << "{\"segment\":" << status.segmentIndex
        << ",\"bytes\":" << status.totalBytes
        << ",\"assigned\":" << status.assignedReplicas
        << ",\"proven\":" << status.provenReplicas
        << ",\"scarcityMultiplierBasisPoints\":"
        << status.scarcityMultiplierBasisPoints << "}";
  }
  oss << "]}";
  return oss.str();
}

} // namespace nodo::archive
