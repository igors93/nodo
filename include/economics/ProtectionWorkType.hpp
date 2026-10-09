#ifndef NODO_ECONOMICS_PROTECTION_WORK_TYPE_HPP
#define NODO_ECONOMICS_PROTECTION_WORK_TYPE_HPP

#include "economics/ValidationWorkRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nodo::economics {

/*
 * The pillars of Proof of Protection. Every reward must trace to measurable
 * work in exactly one pillar. ValidationWorkType stays the per-record ledger
 * vocabulary; this taxonomy groups it for budgeting.
 *
 *   CONSENSUS_PROTECTION    proposing finalized blocks
 *   FINALITY_PARTICIPATION  PRECOMMIT votes inside quorum certificates
 *   NETWORK_AVAILABILITY    relaying and serving data (no verifiable metric
 *                           yet, so it receives no budget)
 *   DATA_AVAILABILITY       integrity and availability challenges
 *   HISTORICAL_ARCHIVAL     Proof of Archival (ADR 0014)
 *   USEFUL_COMPUTE          reserved; never budgeted before a protocol
 *                           upgrade defines verifiable work
 */
enum class ProtectionWorkType {
  CONSENSUS_PROTECTION,
  FINALITY_PARTICIPATION,
  NETWORK_AVAILABILITY,
  DATA_AVAILABILITY,
  HISTORICAL_ARCHIVAL,
  USEFUL_COMPUTE
};

std::string protectionWorkTypeToString(ProtectionWorkType type);

std::optional<ProtectionWorkType>
protectionWorkTypeFor(ValidationWorkType workType);

/*
 * Epoch Protection Budget. The epoch emission cap from EpochEmissionPolicy
 * is the ceiling of everything below; slices that no verifiable work earns
 * are simply not minted. The consensus slice is always the remainder so the
 * split can never exceed the cap.
 */
class ProtectionBudgetSplit {
public:
  static constexpr std::uint32_t kBasisPoints = 10000;
  // Consensus security must keep the majority of the budget.
  static constexpr std::uint32_t kMinConsensusBasisPoints = 6000;

  ProtectionBudgetSplit();
  ProtectionBudgetSplit(std::uint32_t availabilityBasisPoints,
                        std::uint32_t dataProtectionBasisPoints,
                        std::uint32_t archivalBasisPoints);

  // Today's protocol: the whole cap secures consensus.
  static ProtectionBudgetSplit consensusOnly();
  // The split once ADR 0014 archival rewards are activated.
  static ProtectionBudgetSplit withArchival(std::uint32_t archivalBasisPoints);

  std::uint32_t consensusBasisPoints() const;
  std::uint32_t availabilityBasisPoints() const;
  std::uint32_t dataProtectionBasisPoints() const;
  std::uint32_t archivalBasisPoints() const;
  bool isValid() const;

  // Slice of an epoch emission cap; floors, so slices never exceed the cap.
  utils::Amount slice(utils::Amount epochEmissionCap,
                      ProtectionWorkType pillar) const;

private:
  std::uint32_t m_availabilityBasisPoints;
  std::uint32_t m_dataProtectionBasisPoints;
  std::uint32_t m_archivalBasisPoints;
};

} // namespace nodo::economics

#endif
