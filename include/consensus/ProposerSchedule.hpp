#ifndef NODO_CONSENSUS_PROPOSER_SCHEDULE_HPP
#define NODO_CONSENSUS_PROPOSER_SCHEDULE_HPP

#include "core/ValidatorRegistry.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

/*
 * ProposerSchedule selects the block proposer deterministically.
 *
 * Security principle:
 * Proposer selection must be deterministic and unpredictable-ahead-of-time.
 * We derive the index from a hash of chainId + height + round so that no
 * single party can predict the full schedule without knowing future rounds.
 * All nodes must compute the same result given the same inputs.
 */
class ProposerSchedule {
public:
  static std::string selectProposer(const core::ValidatorRegistry &registry,
                                    const std::string &chainId,
                                    std::uint64_t height, std::uint64_t round);

  static std::string buildSelectionKey(const std::string &chainId,
                                       std::uint64_t height,
                                       std::uint64_t round);
};

} // namespace nodo::consensus

#endif
