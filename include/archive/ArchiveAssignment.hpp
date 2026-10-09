#ifndef NODO_ARCHIVE_ARCHIVE_ASSIGNMENT_HPP
#define NODO_ARCHIVE_ARCHIVE_ASSIGNMENT_HPP

#include "archive/ArchiveProvider.hpp"
#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::archive {

struct ArchiveSlot {
  std::uint64_t segmentIndex = 0;
  std::uint32_t replica = 0;
  std::string providerId;
};

/*
 * The network, not the provider, chooses which segments a provider must
 * hold (ADR 0014). For every sealed segment and replica slot the eligible
 * active provider with the lowest H(segment, replica, assignmentKey) wins
 * (rendezvous hashing), subject to:
 *
 *   - declared capacity in bytes;
 *   - slots bounded by bond / archiveMinBondPerSlot, so extra identities
 *     need extra bond and earn nothing per unit of capital;
 *   - at most one slot per segment per provider and per operator, so one
 *     operator can never pose as several independent copies.
 *
 * Rendezvous hashing keeps assignments stable when providers join or leave,
 * so honest providers rarely move data. Pure function of its inputs.
 */
class ArchiveAssignmentPlanner {
public:
  static std::vector<ArchiveSlot>
  plan(const config::HistoryParameters &parameters,
       const std::vector<ArchiveSegmentCommitment> &segments,
       const ArchiveProviderRegistry &registry);

  // Slots assigned to one provider, in segment order.
  static std::vector<ArchiveSlot>
  slotsFor(const std::vector<ArchiveSlot> &slots, const std::string &providerId);
};

} // namespace nodo::archive

#endif
