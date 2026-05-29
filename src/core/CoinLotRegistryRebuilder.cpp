#include "core/CoinLotRegistryRebuilder.hpp"

#include "economics/ProtectionEconomicsRebuilder.hpp"

#include <stdexcept>

namespace nodo::core {

CoinLotRegistry CoinLotRegistryRebuilder::rebuildFromProtectionEconomicsState(
    const economics::ProtectionEconomicsState& state
) {
    if (!state.isValid()) {
        throw std::invalid_argument(
            "Cannot rebuild CoinLotRegistry from invalid protection economics state."
        );
    }

    return CoinLotRegistry::fromCoinLots(
        state.rewardCoinLots()
    );
}

CoinLotRegistry CoinLotRegistryRebuilder::rebuildRewardLotsFromBlockchain(
    const Blockchain& blockchain
) {
    const economics::ProtectionEconomicsState state =
        economics::ProtectionEconomicsRebuilder::rebuildFromBlockchain(
            blockchain
        );

    return rebuildFromProtectionEconomicsState(state);
}

CoinLotRegistry CoinLotRegistryRebuilder::rebuildRewardLotsFromBlocks(
    const std::vector<Block>& blocks
) {
    const economics::ProtectionEconomicsState state =
        economics::ProtectionEconomicsRebuilder::rebuildFromBlocks(
            blocks
        );

    return rebuildFromProtectionEconomicsState(state);
}

} // namespace nodo::core
