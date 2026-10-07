#ifndef NODO_CONSENSUS_CONSENSUS_WEIGHT_HPP
#define NODO_CONSENSUS_CONSENSUS_WEIGHT_HPP

#include <cstdint>

namespace nodo::consensus {

/*
 * ConsensusWeight encapsulates the logic for deriving validator voting weight
 * from active locked stake.
 *
 * One raw unit of active locked stake gives one unit of voting power. Linear
 * weight preserves the same total when a stakeholder divides stake among
 * validator keys. No per-validator truncation or cap is applied here.
 */
class ConsensusWeight {
public:
  static std::uint64_t weightFromStake(std::uint64_t lockedAmount);
};

} // namespace nodo::consensus

#endif
