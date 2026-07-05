#include "consensus/ConsensusWeight.hpp"

namespace nodo::consensus {

std::uint64_t ConsensusWeight::weightFromStake(std::uint64_t lockedAmount) {
  if (lockedAmount == 0) {
    return 0;
  }

  std::uint64_t low = 1;
  // We cap the high value to the maximum possible square root of a uint64_t
  // to avoid overflow when squaring.
  std::uint64_t high =
      lockedAmount < 4'294'967'295ULL ? lockedAmount : 4'294'967'295ULL;
  std::uint64_t answer = 0;

  while (low <= high) {
    const std::uint64_t mid = low + ((high - low) / 2);
    // Using __int128 to safely check mid * mid <= lockedAmount
    const unsigned __int128 square = static_cast<unsigned __int128>(mid) *
                                     static_cast<unsigned __int128>(mid);

    if (square <= lockedAmount) {
      answer = mid;
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  return answer;
}

} // namespace nodo::consensus
