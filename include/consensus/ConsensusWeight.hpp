#ifndef NODO_CONSENSUS_CONSENSUS_WEIGHT_HPP
#define NODO_CONSENSUS_CONSENSUS_WEIGHT_HPP

#include <cstdint>

namespace nodo::consensus {

/*
 * ConsensusWeight encapsulates the logic for deriving validator voting weight
 * from active locked stake.
 *
 * This employs a canonical weightFromStake(lockedAmount) = floor(sqrt(locked))
 * function using deterministic integer arithmetic to avoid floating-point
 * rounding disparities across platforms. This sub-linear scaling represents an
 * anti-plutocracy choice, ensuring large stake pools have diminishing influence
 * over consensus and promoting decentralization.
 */
class ConsensusWeight {
public:
  static std::uint64_t weightFromStake(std::uint64_t lockedAmount);
};

} // namespace nodo::consensus

#endif
