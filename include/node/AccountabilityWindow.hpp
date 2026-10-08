#ifndef NODO_NODE_ACCOUNTABILITY_WINDOW_HPP
#define NODO_NODE_ACCOUNTABILITY_WINDOW_HPP

#include "node/ValidatorLifecycle.hpp"

#include <cstdint>

namespace nodo::node {

// Development protocol limits. Height and time must BOTH mature before a
// withdrawal: a fast chain cannot shorten the calendar hold, and a stalled
// chain cannot release collateral while evidence is still admissible.
struct AccountabilityWindow {
  static constexpr std::uint64_t kEvidenceMaxAgeBlocks =
      21 * NODO_VALIDATOR_EPOCH_BLOCKS;
  static constexpr std::uint64_t kUnbondingBlocks =
      28 * NODO_VALIDATOR_EPOCH_BLOCKS;
  static constexpr std::int64_t kUnbondingSeconds = 28 * 24 * 60 * 60;
  // Current block admission permits up to five minutes of future timestamp.
  static constexpr std::int64_t kFutureBlockSkewSeconds = 300;

  static constexpr bool evidenceHeightIsAdmissible(
      std::uint64_t offenseHeight, std::uint64_t inclusionHeight) {
    return offenseHeight != 0 && offenseHeight < inclusionHeight &&
           inclusionHeight - offenseHeight <= kEvidenceMaxAgeBlocks;
  }
};

static_assert(AccountabilityWindow::kEvidenceMaxAgeBlocks <
              AccountabilityWindow::kUnbondingBlocks);

} // namespace nodo::node

#endif
