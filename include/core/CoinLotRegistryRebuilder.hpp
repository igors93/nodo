#ifndef NODO_CORE_COIN_LOT_REGISTRY_REBUILDER_HPP
#define NODO_CORE_COIN_LOT_REGISTRY_REBUILDER_HPP

#include "core/Blockchain.hpp"
#include "core/Block.hpp"
#include "core/CoinLotRegistry.hpp"
#include "economics/ProtectionEconomicsState.hpp"

#include <vector>

namespace nodo::core {

/*
 * CoinLotRegistryRebuilder creates a CoinLotRegistry from accepted chain
 * history.
 *
 * Current scope:
 * It starts with GenesisReward coin lots produced by the protection economics
 * state rebuilder.
 *
 * Future scope:
 * It should also apply transaction input/output lot movement, stake locks,
 * slashing, and private accounting proofs.
 */
class CoinLotRegistryRebuilder {
public:
    static CoinLotRegistry rebuildFromProtectionEconomicsState(
        const economics::ProtectionEconomicsState& state
    );

    static CoinLotRegistry rebuildRewardLotsFromBlockchain(
        const Blockchain& blockchain
    );

    static CoinLotRegistry rebuildRewardLotsFromBlocks(
        const std::vector<Block>& blocks
    );
};

} // namespace nodo::core

#endif
