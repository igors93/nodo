#ifndef NODO_CONSENSUS_CONSENSUS_RECOVERY_STORE_HPP
#define NODO_CONSENSUS_CONSENSUS_RECOVERY_STORE_HPP

#include "consensus/ConsensusRoundManager.hpp"

#include <filesystem>
#include <optional>

namespace nodo::consensus {

/*
 * ConsensusRecoveryStore persists the minimum round state needed to resume
 * consensus after a node restart.
 *
 * Security principle:
 * On restart, a node must not start from round 0 if it was already in the
 * middle of a round. Doing so could cause it to vote for two different blocks
 * at the same height. The recovery store saves the last known round state so
 * the node can reload it and skip rounds it has already participated in.
 */
class ConsensusRecoveryStore {
public:
    static bool save(
        const std::filesystem::path& path,
        const ConsensusRoundState& state
    );

    static std::optional<ConsensusRoundState> load(
        const std::filesystem::path& path
    );

    static bool remove(
        const std::filesystem::path& path
    );
};

} // namespace nodo::consensus

#endif
