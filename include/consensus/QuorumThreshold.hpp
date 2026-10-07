#ifndef NODO_CONSENSUS_QUORUM_THRESHOLD_HPP
#define NODO_CONSENSUS_QUORUM_THRESHOLD_HPP

#include <cstdint>
#include <stdexcept>

namespace nodo::consensus {

// The only quorum rule for this protocol version. A configurable higher
// fraction would change the liveness fault bound; a lower one would weaken
// quorum intersection. Keep the serialized network fields canonical.
class QuorumThreshold {
public:
  static constexpr std::uint64_t kNumerator = 2;
  static constexpr std::uint64_t kDenominator = 3;

  static constexpr bool isCanonicalFraction(std::uint64_t numerator,
                                            std::uint64_t denominator) {
    return numerator == kNumerator && denominator == kDenominator;
  }

  static std::uint64_t requiredWeight(std::uint64_t totalWeight) {
    if (totalWeight == 0) {
      throw std::invalid_argument("Quorum requires positive total voting weight.");
    }

    // Exactly floor(2 * totalWeight / 3) + 1, without multiplication or
    // addition that could overflow at UINT64_MAX.
    return totalWeight - ((totalWeight - 1) / kDenominator);
  }
};

} // namespace nodo::consensus

#endif
