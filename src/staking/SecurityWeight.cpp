#include "staking/SecurityWeight.hpp"

namespace nodo::staking {

std::uint64_t SecurityWeight::calculateForCoinLot(
    const core::CoinLot& coinLot,
    std::uint64_t currentBlock
) {
    if (!coinLot.isLockedForSecurity()) {
        return 0;
    }

    const std::uint64_t amountInNodo =
        static_cast<std::uint64_t>(
            coinLot.amount().rawUnits() / utils::Amount::UNITS_PER_NODO
        );

    const std::uint64_t multiplier =
        lockDurationMultiplier(currentBlock, coinLot.lockedUntilBlock());

    return amountInNodo * multiplier;
}

std::uint64_t SecurityWeight::lockDurationMultiplier(
    std::uint64_t currentBlock,
    std::uint64_t lockedUntilBlock
) {
    if (lockedUntilBlock <= currentBlock) {
        return 1;
    }

    const std::uint64_t remainingBlocks = lockedUntilBlock - currentBlock;

    /*
     * Regra inicial simples:
     *
     * 0-99 blocos restantes     → multiplicador 1
     * 100-499 blocos restantes  → multiplicador 2
     * 500+ blocos restantes     → multiplicador 3
     *
     * Isso incentiva travamentos mais longos,
     * mas evita crescimento exagerado no início.
     */
    if (remainingBlocks >= 500) {
        return 3;
    }

    if (remainingBlocks >= 100) {
        return 2;
    }

    return 1;
}

} // namespace nodo::staking