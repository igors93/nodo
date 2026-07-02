#ifndef NODO_NODE_VALIDATOR_STAKE_WEIGHT_UPDATER_HPP
#define NODO_NODE_VALIDATOR_STAKE_WEIGHT_UPDATER_HPP

#include <cstdint>

namespace nodo::core {
class ValidatorRegistry;
}

namespace nodo::node {

class StakingRegistry;

/*
 * Projects the authoritative active stake ledger into the validator registry
 * exactly at validator-epoch boundaries. Consensus reads only the registry,
 * so stake changes remain deterministic and stable for the duration of an
 * epoch while the staking domain can continue tracking pending lifecycle
 * operations independently.
 */
class ValidatorStakeWeightUpdater {
public:
    static bool isEpochBoundary(std::uint64_t finalizedBlockHeight);

    static std::uint64_t effectiveEpochForBoundary(
        std::uint64_t finalizedBlockHeight
    );

    // Returns false without mutating either registry when the height is not an
    // epoch boundary. Invalid state throws before the caller-visible registry
    // is changed; reconciliation itself is performed on a copy.
    static bool synchronizeAtEpochBoundary(
        std::uint64_t finalizedBlockHeight,
        std::int64_t blockTimestamp,
        const StakingRegistry& staking,
        core::ValidatorRegistry& validators
    );
};

} // namespace nodo::node

#endif
