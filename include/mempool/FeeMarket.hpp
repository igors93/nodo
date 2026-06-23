#ifndef NODO_MEMPOOL_FEE_MARKET_HPP
#define NODO_MEMPOOL_FEE_MARKET_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::mempool {

enum class FeeUrgency { LOW, MEDIUM, HIGH };

struct FeeMarketState {
    utils::Amount baseFee;
    std::uint64_t targetBlockCapacity;  // target transactions per block
    std::uint64_t lastBlockTxCount;     // actual tx count of last block
    std::uint64_t blockHeight;

    std::string serialize() const;
};

/*
 * FeeMarket implements an EIP-1559-style dynamic base fee mechanism.
 *
 * Security principle:
 * The base fee is a protocol-enforced floor. Transactions that do not meet
 * the current base fee must be excluded from block production. This prevents
 * spam during congestion without requiring mempool policy changes.
 */
class FeeMarket {
public:
    // Minimum base fee (floor) — 100 raw units
    static constexpr std::int64_t MINIMUM_BASE_FEE_RAW = 100;
    // Max change per block: 12.5% up or down (125/1000)
    static constexpr std::uint32_t MAX_FEE_CHANGE_NUMERATOR   = 125;
    static constexpr std::uint32_t MAX_FEE_CHANGE_DENOMINATOR = 1000;

    // Compute next block's base fee given last block's utilization.
    // If lastBlockTxCount > targetCapacity  → fee increases (up to +12.5%)
    // If lastBlockTxCount < targetCapacity  → fee decreases (up to -12.5%)
    // If lastBlockTxCount == targetCapacity → fee unchanged
    static utils::Amount computeNextBaseFee(
        utils::Amount currentBaseFee,
        std::uint64_t lastBlockTxCount,
        std::uint64_t targetBlockCapacity
    );

    // Returns true if txFee >= baseFee
    static bool isFeeSufficient(
        utils::Amount txFee,
        utils::Amount baseFee
    );

    // Estimate fee for urgency level:
    // LOW    = baseFee × 1.0 (exactly base, may not be included quickly)
    // MEDIUM = baseFee × 1.1
    // HIGH   = baseFee × 1.25
    static utils::Amount estimateFee(
        FeeUrgency urgency,
        utils::Amount baseFee
    );

    // Initial state for genesis or first block
    static FeeMarketState initialState(
        std::uint64_t targetBlockCapacity
    );

    // Advance state to next block
    static FeeMarketState advanceState(
        const FeeMarketState& current,
        std::uint64_t actualTxCount
    );
};

} // namespace nodo::mempool

#endif
