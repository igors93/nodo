#include "staking/SecurityWeight.hpp"

#include <limits>

namespace nodo::staking {

std::uint64_t SecurityWeight::calculateForCoinLot(
    const core::CoinLot& coinLot,
    std::uint64_t currentBlock
) {
    if (!coinLot.isLockedForSecurity()) {
        return 0;
    }

    const std::uint64_t multiplier =
        lockDurationMultiplier(currentBlock, coinLot.lockedUntilBlock());

    // Use __int128 to multiply before dividing without overflow risk.
    const __int128 product =
        static_cast<__int128>(coinLot.amount().rawUnits())
        * static_cast<__int128>(multiplier);
    const __int128 result = product / static_cast<__int128>(utils::Amount::UNITS_PER_NODO);
    constexpr __int128 kMax = static_cast<__int128>(std::numeric_limits<std::uint64_t>::max());
    return static_cast<std::uint64_t>(result > kMax ? kMax : result);
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