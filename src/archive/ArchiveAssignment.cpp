#include "archive/ArchiveAssignment.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <map>
#include <set>
#include <stdexcept>

namespace nodo::archive {

namespace {

struct Budget {
  std::uint64_t remainingBytes = 0;
  std::uint64_t remainingSlots = 0;
};

std::string rank(std::uint64_t segmentIndex, std::uint32_t replica,
                 const std::string &assignmentKey) {
  serialization::CanonicalWriter writer;
  writer.writeUInt64(segmentIndex);
  writer.writeUInt32(replica);
  writer.writeString(assignmentKey);
  return hashHex("ARCHIVE/ASSIGN", writer.bytes());
}

} // namespace

std::vector<ArchiveSlot> ArchiveAssignmentPlanner::plan(
    const config::HistoryParameters &parameters,
    const std::vector<ArchiveSegmentCommitment> &segments,
    const ArchiveProviderRegistry &registry) {
  if (!parameters.isValid() || !ArchiveIndex::isContiguous(segments)) {
    throw std::invalid_argument("Assignment needs contiguous sealed segments.");
  }
  const std::vector<const ArchiveProviderRecord *> providers =
      registry.activeProviders();
  std::map<std::string, Budget> budgets;
  for (const ArchiveProviderRecord *provider : providers) {
    const ArchiveProviderRegistrationFields &fields =
        provider->registration.fields();
    budgets[fields.providerId] =
        Budget{fields.declaredCapacityBytes,
               provider->registration.maxSlots(parameters)};
  }

  std::vector<ArchiveSlot> slots;
  for (const ArchiveSegmentCommitment &segment : segments) {
    std::set<std::string> holders;
    std::set<std::string> operators;
    for (std::uint32_t replica = 0;
         replica < parameters.archiveReplicationTarget(); ++replica) {
      const ArchiveProviderRecord *best = nullptr;
      std::string bestRank;
      for (const ArchiveProviderRecord *provider : providers) {
        const ArchiveProviderRegistrationFields &fields =
            provider->registration.fields();
        const Budget &budget = budgets[fields.providerId];
        if (budget.remainingSlots == 0 ||
            budget.remainingBytes < segment.totalBytes() ||
            holders.count(fields.providerId) != 0 ||
            operators.count(fields.operatorId) != 0) {
          continue;
        }
        const std::string score =
            rank(segment.segmentIndex(), replica, provider->assignmentKey);
        if (best == nullptr || score < bestRank ||
            (score == bestRank &&
             fields.providerId < best->registration.fields().providerId)) {
          best = provider;
          bestRank = score;
        }
      }
      if (best == nullptr) {
        break; // Under-replicated: reported by ArchiveReplicationReport.
      }
      const std::string &winner = best->registration.fields().providerId;
      Budget &budget = budgets[winner];
      budget.remainingBytes -= segment.totalBytes();
      --budget.remainingSlots;
      holders.insert(winner);
      operators.insert(best->registration.fields().operatorId);
      slots.push_back(ArchiveSlot{segment.segmentIndex(), replica, winner});
    }
  }
  return slots;
}

std::vector<ArchiveSlot>
ArchiveAssignmentPlanner::slotsFor(const std::vector<ArchiveSlot> &slots,
                                   const std::string &providerId) {
  std::vector<ArchiveSlot> mine;
  for (const ArchiveSlot &slot : slots) {
    if (slot.providerId == providerId) {
      mine.push_back(slot);
    }
  }
  return mine;
}

} // namespace nodo::archive
