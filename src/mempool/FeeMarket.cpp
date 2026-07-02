#include "mempool/FeeMarket.hpp"

#include <algorithm>
#include <sstream>

namespace nodo::mempool {

std::string FeeMarketState::serialize() const {
  std::ostringstream oss;
  oss << "FeeMarketState{"
      << "baseFeeRaw=" << baseFee.rawUnits()
      << ";targetBlockCapacity=" << targetBlockCapacity
      << ";lastBlockTxCount=" << lastBlockTxCount
      << ";blockHeight=" << blockHeight << "}";
  return oss.str();
}

utils::Amount FeeMarket::computeNextBaseFee(utils::Amount currentBaseFee,
                                            std::uint64_t lastBlockTxCount,
                                            std::uint64_t targetBlockCapacity) {
  const std::int64_t current = currentBaseFee.rawUnits();

  if (targetBlockCapacity == 0) {
    // Degenerate: no target capacity, keep fee unchanged but enforce floor
    const std::int64_t floored = std::max(current, MINIMUM_BASE_FEE_RAW);
    return utils::Amount::fromRawUnits(floored);
  }

  // Compute the maximum delta allowed per block (12.5% of current fee)
  const std::int64_t maxDelta =
      (current * static_cast<std::int64_t>(MAX_FEE_CHANGE_NUMERATOR)) /
      static_cast<std::int64_t>(MAX_FEE_CHANGE_DENOMINATOR);

  std::int64_t nextFee = current;

  if (lastBlockTxCount > targetBlockCapacity) {
    // Over target: increase proportionally, cap at maxDelta
    // delta = current * (lastBlockTxCount - targetBlockCapacity) /
    // targetBlockCapacity but capped at maxDelta
    const std::uint64_t excess = lastBlockTxCount - targetBlockCapacity;
    const std::int64_t delta = std::min(
        maxDelta,
        static_cast<std::int64_t>(
            (static_cast<__int128>(current) * static_cast<__int128>(excess)) /
            static_cast<__int128>(targetBlockCapacity)));

    // Ensure at least a 1-unit increase when over target
    nextFee = current + std::max(delta, static_cast<std::int64_t>(1));

  } else if (lastBlockTxCount < targetBlockCapacity) {
    // Under target: decrease proportionally, cap at maxDelta
    const std::uint64_t deficit = targetBlockCapacity - lastBlockTxCount;
    const std::int64_t delta = std::min(
        maxDelta,
        static_cast<std::int64_t>(
            (static_cast<__int128>(current) * static_cast<__int128>(deficit)) /
            static_cast<__int128>(targetBlockCapacity)));

    nextFee = current - delta;
  }
  // else lastBlockTxCount == targetBlockCapacity → fee unchanged

  // Enforce floor
  nextFee = std::max(nextFee, MINIMUM_BASE_FEE_RAW);

  return utils::Amount::fromRawUnits(nextFee);
}

bool FeeMarket::isFeeSufficient(utils::Amount txFee, utils::Amount baseFee) {
  return txFee >= baseFee;
}

utils::Amount FeeMarket::estimateFee(FeeUrgency urgency,
                                     utils::Amount baseFee) {
  const std::int64_t base = baseFee.rawUnits();

  switch (urgency) {
  case FeeUrgency::LOW:
    // 1.0x — exactly base
    return utils::Amount::fromRawUnits(base);

  case FeeUrgency::MEDIUM: {
    // 1.1x — base * 110 / 100
    const __int128 estimated = (static_cast<__int128>(base) * 110) / 100;
    if (estimated > INT64_MAX) {
      throw std::overflow_error("Fee estimation overflow.");
    }
    return utils::Amount::fromRawUnits(static_cast<std::int64_t>(estimated));
  }

  case FeeUrgency::HIGH: {
    // 1.25x — base * 125 / 100
    const __int128 estimated = (static_cast<__int128>(base) * 125) / 100;
    if (estimated > INT64_MAX) {
      throw std::overflow_error("Fee estimation overflow.");
    }
    return utils::Amount::fromRawUnits(static_cast<std::int64_t>(estimated));
  }

  default:
    return utils::Amount::fromRawUnits(base);
  }
}

FeeMarketState FeeMarket::initialState(std::uint64_t targetBlockCapacity) {
  FeeMarketState state;
  state.baseFee = utils::Amount::fromRawUnits(MINIMUM_BASE_FEE_RAW);
  state.targetBlockCapacity = targetBlockCapacity;
  state.lastBlockTxCount = 0;
  state.blockHeight = 0;
  return state;
}

FeeMarketState FeeMarket::advanceState(const FeeMarketState &current,
                                       std::uint64_t actualTxCount) {
  FeeMarketState next;
  next.targetBlockCapacity = current.targetBlockCapacity;
  next.lastBlockTxCount = actualTxCount;
  next.blockHeight = current.blockHeight + 1;
  next.baseFee = computeNextBaseFee(current.baseFee, actualTxCount,
                                    current.targetBlockCapacity);
  return next;
}

} // namespace nodo::mempool
