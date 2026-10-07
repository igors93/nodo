#include "consensus/ConsensusWeight.hpp"

namespace nodo::consensus {

std::uint64_t ConsensusWeight::weightFromStake(std::uint64_t lockedAmount) {
  return lockedAmount;
}

} // namespace nodo::consensus
