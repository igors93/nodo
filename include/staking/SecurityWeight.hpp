#ifndef NODO_STAKING_SECURITY_WEIGHT_HPP
#define NODO_STAKING_SECURITY_WEIGHT_HPP

#include "core/CoinLot.hpp"

#include <cstdint>

namespace nodo::staking {

/* SecurityWeight measures the economic weight of locked coin lots. */
class SecurityWeight {
public:
    /* Calculate a lot's deterministic security weight from its locked amount and remaining lock duration. */
    static std::uint64_t calculateForCoinLot(
        const core::CoinLot& coinLot,
        std::uint64_t currentBlock
    );

private:
    static std::uint64_t lockDurationMultiplier(
        std::uint64_t currentBlock,
        std::uint64_t lockedUntilBlock
    );
};

} // namespace nodo::staking

#endif