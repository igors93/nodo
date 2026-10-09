#ifndef NODO_CONSENSUS_PROPOSER_SCHEDULE_HPP
#define NODO_CONSENSUS_PROPOSER_SCHEDULE_HPP

#include "core/ValidatorRegistry.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

/*
 * ProposerSchedule selects the block proposer deterministically.
 *
 * Development protocol only. This modulo-hash lottery is public and
 * predictable from chain ID, height, round and the set, and it has modulo
 * bias. It is not the v1 proposer rule. V1 uses the separately specified
 * authenticated weighted-priority schedule in ADR 0012.
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
