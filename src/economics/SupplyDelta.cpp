#include "economics/SupplyDelta.hpp"

#include <climits>
#include <sstream>
#include <utility>

namespace nodo::economics {

SupplyDelta::SupplyDelta()
    : m_blockHeight(0),
      m_blockHash(""),
      m_epoch(0),
      m_supplyBefore(utils::Amount::fromRawUnits(0)),
      m_mintedAmount(utils::Amount::fromRawUnits(0)),
      m_burnedAmount(utils::Amount::fromRawUnits(0)),
      m_supplyAfter(utils::Amount::fromRawUnits(0)) {}

SupplyDelta::SupplyDelta(
    std::uint64_t blockHeight,
    std::string blockHash,
    std::uint64_t epoch,
    utils::Amount supplyBefore,
    utils::Amount mintedAmount,
    utils::Amount burnedAmount,
    utils::Amount supplyAfter,
    std::vector<MintRecord> mintRecords,
    std::vector<BurnRecord> burnRecords
)
    : m_blockHeight(blockHeight),
      m_blockHash(std::move(blockHash)),
      m_epoch(epoch),
      m_supplyBefore(supplyBefore),
      m_mintedAmount(mintedAmount),
      m_burnedAmount(burnedAmount),
      m_supplyAfter(supplyAfter),
      m_mintRecords(std::move(mintRecords)),
      m_burnRecords(std::move(burnRecords)) {}

SupplyDelta SupplyDelta::noOp(
    std::uint64_t blockHeight,
    const std::string& blockHash,
    std::uint64_t epoch,
    utils::Amount currentSupply
) {
    return SupplyDelta(
        blockHeight,
        blockHash,
        epoch,
        currentSupply,
        utils::Amount::fromRawUnits(0),
        utils::Amount::fromRawUnits(0),
        currentSupply,
        {},
        {}
    );
}

std::uint64_t SupplyDelta::blockHeight() const { return m_blockHeight; }
const std::string& SupplyDelta::blockHash() const { return m_blockHash; }
std::uint64_t SupplyDelta::epoch() const { return m_epoch; }
utils::Amount SupplyDelta::supplyBefore() const { return m_supplyBefore; }
utils::Amount SupplyDelta::mintedAmount() const { return m_mintedAmount; }
utils::Amount SupplyDelta::burnedAmount() const { return m_burnedAmount; }
utils::Amount SupplyDelta::supplyAfter() const { return m_supplyAfter; }
const std::vector<MintRecord>& SupplyDelta::mintRecords() const { return m_mintRecords; }
const std::vector<BurnRecord>& SupplyDelta::burnRecords() const { return m_burnRecords; }

bool SupplyDelta::isValid() const {
    return rejectionReason().empty();
}

std::string SupplyDelta::rejectionReason() const {
    if (m_blockHash.empty()) {
        return "SupplyDelta rejected: blockHash is empty.";
    }
    if (m_supplyBefore.isNegative()) {
        return "SupplyDelta rejected: supplyBefore is negative.";
    }
    if (m_mintedAmount.isNegative()) {
        return "SupplyDelta rejected: mintedAmount is negative.";
    }
    if (m_burnedAmount.isNegative()) {
        return "SupplyDelta rejected: burnedAmount is negative.";
    }
    if (m_supplyAfter.isNegative()) {
        return "SupplyDelta rejected: supplyAfter is negative (supply underflow).";
    }

    // Validate individual mint records and sum them.
    std::int64_t mintSum = 0;
    for (const auto& mint : m_mintRecords) {
        if (!mint.isValid()) {
            return "SupplyDelta rejected: invalid MintRecord id='" + mint.id() + "'.";
        }
        // Consistency: every mint must belong to the same block and epoch as this delta.
        if (mint.epoch() != m_epoch) {
            return "SupplyDelta rejected: MintRecord id='" + mint.id() +
                   "' epoch " + std::to_string(mint.epoch()) +
                   " does not match delta epoch " + std::to_string(m_epoch) + ".";
        }
        if (mint.sourceBlockIndex() != m_blockHeight) {
            return "SupplyDelta rejected: MintRecord id='" + mint.id() +
                   "' sourceBlockIndex " + std::to_string(mint.sourceBlockIndex()) +
                   " does not match delta blockHeight " + std::to_string(m_blockHeight) + ".";
        }
        if (mint.sourceBlockHash() != m_blockHash) {
            return "SupplyDelta rejected: MintRecord id='" + mint.id() +
                   "' sourceBlockHash '" + mint.sourceBlockHash() +
                   "' does not match delta blockHash '" + m_blockHash + "'.";
        }
        // Overflow check: mintSum + mint.amount().rawUnits() must not overflow.
        if (mint.amount().rawUnits() > 0 &&
            mintSum > INT64_MAX - mint.amount().rawUnits()) {
            return "SupplyDelta rejected: mint records sum overflows int64.";
        }
        mintSum += mint.amount().rawUnits();
    }
    if (mintSum != m_mintedAmount.rawUnits()) {
        return "SupplyDelta rejected: mintedAmount (" +
               std::to_string(m_mintedAmount.rawUnits()) +
               ") does not equal sum of mint records (" +
               std::to_string(mintSum) + ").";
    }

    // Validate individual burn records and sum them.
    std::int64_t burnSum = 0;
    for (const auto& burn : m_burnRecords) {
        if (!burn.isValid()) {
            return "SupplyDelta rejected: invalid BurnRecord burnId='" +
                   burn.burnId() + "': " + burn.rejectionReason();
        }
        // Consistency: every burn must belong to the same block and epoch as this delta.
        if (burn.epoch() != m_epoch) {
            return "SupplyDelta rejected: BurnRecord burnId='" + burn.burnId() +
                   "' epoch " + std::to_string(burn.epoch()) +
                   " does not match delta epoch " + std::to_string(m_epoch) + ".";
        }
        if (burn.blockHeight() != m_blockHeight) {
            return "SupplyDelta rejected: BurnRecord burnId='" + burn.burnId() +
                   "' blockHeight " + std::to_string(burn.blockHeight()) +
                   " does not match delta blockHeight " + std::to_string(m_blockHeight) + ".";
        }
        if (burn.amount().rawUnits() > 0 &&
            burnSum > INT64_MAX - burn.amount().rawUnits()) {
            return "SupplyDelta rejected: burn records sum overflows int64.";
        }
        burnSum += burn.amount().rawUnits();
    }
    if (burnSum != m_burnedAmount.rawUnits()) {
        return "SupplyDelta rejected: burnedAmount (" +
               std::to_string(m_burnedAmount.rawUnits()) +
               ") does not equal sum of burn records (" +
               std::to_string(burnSum) + ").";
    }

    // Verify the supply equation.
    // supplyAfter == supplyBefore + mintedAmount - burnedAmount
    // Rearranged to avoid creating intermediate negative: check as int64
    const std::int64_t before = m_supplyBefore.rawUnits();
    const std::int64_t minted = m_mintedAmount.rawUnits();
    const std::int64_t burned = m_burnedAmount.rawUnits();
    const std::int64_t after  = m_supplyAfter.rawUnits();

    // Overflow check for before + minted
    if (minted > 0 && before > INT64_MAX - minted) {
        return "SupplyDelta rejected: supplyBefore + mintedAmount overflows int64.";
    }
    const std::int64_t intermediate = before + minted;

    // Underflow check
    if (intermediate < burned) {
        return "SupplyDelta rejected: supply underflow "
               "(supplyBefore + mintedAmount < burnedAmount).";
    }

    const std::int64_t expected = intermediate - burned;
    if (after != expected) {
        return "SupplyDelta rejected: supply equation mismatch: "
               "supplyBefore(" + std::to_string(before) + ") + "
               "minted(" + std::to_string(minted) + ") - "
               "burned(" + std::to_string(burned) + ") = " +
               std::to_string(expected) + " but supplyAfter = " +
               std::to_string(after) + ".";
    }

    return "";
}

std::string SupplyDelta::serialize() const {
    std::ostringstream oss;
    oss << "SupplyDelta{"
        << "blockHeight=" << m_blockHeight
        << ";blockHash=" << m_blockHash
        << ";epoch=" << m_epoch
        << ";supplyBeforeRaw=" << m_supplyBefore.rawUnits()
        << ";mintedRaw=" << m_mintedAmount.rawUnits()
        << ";burnedRaw=" << m_burnedAmount.rawUnits()
        << ";supplyAfterRaw=" << m_supplyAfter.rawUnits()
        << ";mintCount=" << m_mintRecords.size()
        << ";burnCount=" << m_burnRecords.size()
        << "}";
    return oss.str();
}

} // namespace nodo::economics
