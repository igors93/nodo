#include "node/ValidatorStakeWeightUpdater.hpp"

#include "core/ValidatorRegistry.hpp"
#include "node/StakingRegistry.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

bool ValidatorStakeWeightUpdater::isEpochBoundary(
    std::uint64_t finalizedBlockHeight
) {
    return finalizedBlockHeight > 0 &&
           finalizedBlockHeight % NODO_VALIDATOR_EPOCH_BLOCKS == 0;
}

std::uint64_t ValidatorStakeWeightUpdater::effectiveEpochForBoundary(
    std::uint64_t finalizedBlockHeight
) {
    if (!isEpochBoundary(finalizedBlockHeight)) {
        throw std::invalid_argument(
            "Validator weight update height is not an epoch boundary."
        );
    }

    const std::uint64_t completedEpoch =
        ValidatorLifecycle::epochIndexForBlock(finalizedBlockHeight);
    if (completedEpoch == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Effective validator epoch would overflow.");
    }
    return completedEpoch + 1;
}

bool ValidatorStakeWeightUpdater::synchronizeAtEpochBoundary(
    std::uint64_t finalizedBlockHeight,
    std::int64_t blockTimestamp,
    const StakingRegistry& staking,
    core::ValidatorRegistry& validators
) {
    if (finalizedBlockHeight == 0 || blockTimestamp <= 0) {
        throw std::invalid_argument(
            "Validator weight reconciliation requires a finalized height and timestamp."
        );
    }
    if (!isEpochBoundary(finalizedBlockHeight)) {
        return false;
    }
    if (!staking.isValid()) {
        throw std::invalid_argument(
            "Validator weight reconciliation received an invalid staking registry."
        );
    }
    if (!validators.isValid()) {
        throw std::invalid_argument(
            "Validator weight reconciliation received an invalid validator registry."
        );
    }

    core::ValidatorRegistry projected = validators;
    for (const std::string& address : validators.validatorAddresses()) {
        const core::ValidatorRegistryEntry* current =
            validators.entryForAddress(address);
        if (current == nullptr) {
            throw std::logic_error(
                "Validator registry enumeration returned an unknown address."
            );
        }

        const std::int64_t activeStake =
            staking.activeStakeFor(address).rawUnits();
        if (activeStake < 0) {
            throw std::logic_error(
                "Validator active stake cannot be negative."
            );
        }
        if (current->stakeAmount() == static_cast<std::uint64_t>(activeStake)) {
            continue;
        }

        const core::ValidatorRegistryUpdateResult update =
            projected.updateStake(
                address,
                static_cast<std::uint64_t>(activeStake),
                blockTimestamp
            );
        if (!update.success()) {
            throw std::logic_error(
                "Validator stake could not be projected into the epoch set: " +
                update.reason()
            );
        }
    }

    if (!projected.isValid()) {
        throw std::logic_error(
            "Epoch validator weight projection produced an invalid registry."
        );
    }

    validators = std::move(projected);
    return true;
}

} // namespace nodo::node
