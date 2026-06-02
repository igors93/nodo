#include "economics/TreasurySpendRecord.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

TreasurySpendRecord::TreasurySpendRecord()
    : m_amount(utils::Amount::fromRawUnits(0)),
      m_executedAtBlock(0),
      m_epoch(0),
      m_treasuryBalanceBefore(utils::Amount::fromRawUnits(0)),
      m_treasuryBalanceAfter(utils::Amount::fromRawUnits(0)),
      m_valid(false),
      m_rejectionReason("TreasurySpendRecord: default-constructed.") {}

TreasurySpendRecord::TreasurySpendRecord(
    std::string spendId,
    std::string proposalId,
    std::string recipientAddress,
    utils::Amount amount,
    std::string purpose,
    std::uint64_t executedAtBlock,
    std::uint64_t epoch,
    utils::Amount treasuryBalanceBefore,
    utils::Amount treasuryBalanceAfter
)
    : m_spendId(std::move(spendId)),
      m_proposalId(std::move(proposalId)),
      m_recipientAddress(std::move(recipientAddress)),
      m_amount(amount),
      m_purpose(std::move(purpose)),
      m_executedAtBlock(executedAtBlock),
      m_epoch(epoch),
      m_treasuryBalanceBefore(treasuryBalanceBefore),
      m_treasuryBalanceAfter(treasuryBalanceAfter),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_spendId.empty()) {
        m_rejectionReason = "TreasurySpendRecord: spendId must not be empty.";
        return;
    }
    if (m_proposalId.empty()) {
        m_rejectionReason = "TreasurySpendRecord: proposalId must not be empty.";
        return;
    }
    if (m_recipientAddress.empty()) {
        m_rejectionReason = "TreasurySpendRecord: recipientAddress must not be empty.";
        return;
    }
    if (!m_amount.isPositive()) {
        m_rejectionReason = "TreasurySpendRecord: amount must be positive.";
        return;
    }
    if (m_treasuryBalanceBefore.isNegative()) {
        m_rejectionReason = "TreasurySpendRecord: treasuryBalanceBefore must not be negative.";
        return;
    }
    if (m_treasuryBalanceAfter.isNegative()) {
        m_rejectionReason = "TreasurySpendRecord: treasuryBalanceAfter must not be negative (would create negative treasury).";
        return;
    }
    // balanceAfter must equal balanceBefore - amount.
    const std::int64_t expectedAfter =
        m_treasuryBalanceBefore.rawUnits() - m_amount.rawUnits();
    if (expectedAfter != m_treasuryBalanceAfter.rawUnits()) {
        m_rejectionReason =
            "TreasurySpendRecord: balance mismatch: "
            "before(" + std::to_string(m_treasuryBalanceBefore.rawUnits()) +
            ") - amount(" + std::to_string(m_amount.rawUnits()) +
            ") = " + std::to_string(expectedAfter) +
            " != after(" + std::to_string(m_treasuryBalanceAfter.rawUnits()) + ").";
        return;
    }
    m_valid = true;
}

const std::string& TreasurySpendRecord::spendId() const { return m_spendId; }
const std::string& TreasurySpendRecord::proposalId() const { return m_proposalId; }
const std::string& TreasurySpendRecord::recipientAddress() const { return m_recipientAddress; }
utils::Amount TreasurySpendRecord::amount() const { return m_amount; }
const std::string& TreasurySpendRecord::purpose() const { return m_purpose; }
std::uint64_t TreasurySpendRecord::executedAtBlock() const { return m_executedAtBlock; }
std::uint64_t TreasurySpendRecord::epoch() const { return m_epoch; }
utils::Amount TreasurySpendRecord::treasuryBalanceBefore() const { return m_treasuryBalanceBefore; }
utils::Amount TreasurySpendRecord::treasuryBalanceAfter() const { return m_treasuryBalanceAfter; }
bool TreasurySpendRecord::isValid() const { return m_valid; }
const std::string& TreasurySpendRecord::rejectionReason() const { return m_rejectionReason; }

std::string TreasurySpendRecord::serialize() const {
    std::ostringstream oss;
    oss << "TreasurySpendRecord{"
        << "spendId=" << m_spendId
        << ";proposalId=" << m_proposalId
        << ";recipient=" << m_recipientAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";executedAtBlock=" << m_executedAtBlock
        << ";epoch=" << m_epoch
        << ";balanceBeforeRaw=" << m_treasuryBalanceBefore.rawUnits()
        << ";balanceAfterRaw=" << m_treasuryBalanceAfter.rawUnits()
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
