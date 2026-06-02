#include "economics/TreasuryAccount.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace nodo::economics {

std::string treasuryDebitStatusToString(TreasuryDebitStatus status) {
    switch (status) {
        case TreasuryDebitStatus::ACCEPTED:             return "ACCEPTED";
        case TreasuryDebitStatus::LOCKED:               return "LOCKED";
        case TreasuryDebitStatus::INSUFFICIENT_BALANCE: return "INSUFFICIENT_BALANCE";
        case TreasuryDebitStatus::NEGATIVE_AMOUNT:      return "NEGATIVE_AMOUNT";
        default:                                         return "UNKNOWN";
    }
}

TreasuryDebitResult::TreasuryDebitResult()
    : m_status(TreasuryDebitStatus::INSUFFICIENT_BALANCE),
      m_reason("Uninitialized."),
      m_balanceAfter(utils::Amount::fromRawUnits(0)) {}

TreasuryDebitResult TreasuryDebitResult::accepted(utils::Amount balanceAfter) {
    TreasuryDebitResult r;
    r.m_status = TreasuryDebitStatus::ACCEPTED;
    r.m_reason = "";
    r.m_balanceAfter = balanceAfter;
    return r;
}

TreasuryDebitResult TreasuryDebitResult::locked() {
    TreasuryDebitResult r;
    r.m_status = TreasuryDebitStatus::LOCKED;
    r.m_reason = "treasury account is locked";
    return r;
}

TreasuryDebitResult TreasuryDebitResult::insufficientBalance(
    utils::Amount available, utils::Amount requested
) {
    TreasuryDebitResult r;
    r.m_status = TreasuryDebitStatus::INSUFFICIENT_BALANCE;
    r.m_reason = "insufficient balance: available=" +
                 std::to_string(available.rawUnits()) +
                 " requested=" + std::to_string(requested.rawUnits());
    return r;
}

TreasuryDebitResult TreasuryDebitResult::negativeAmount() {
    TreasuryDebitResult r;
    r.m_status = TreasuryDebitStatus::NEGATIVE_AMOUNT;
    r.m_reason = "debit amount must be positive";
    return r;
}

bool TreasuryDebitResult::accepted() const {
    return m_status == TreasuryDebitStatus::ACCEPTED;
}

TreasuryDebitStatus TreasuryDebitResult::status() const {
    return m_status;
}

const std::string& TreasuryDebitResult::reason() const {
    return m_reason;
}

utils::Amount TreasuryDebitResult::balanceAfter() const {
    return m_balanceAfter;
}

TreasuryAccount::TreasuryAccount()
    : m_balance(utils::Amount::fromRawUnits(0)),
      m_epoch(0),
      m_locked(false),
      m_valid(false),
      m_rejectionReason("TreasuryAccount: default-constructed.") {}

TreasuryAccount::TreasuryAccount(
    std::string treasuryId,
    std::string accountAddress,
    utils::Amount balance,
    std::uint64_t epoch,
    bool locked,
    std::string reason
)
    : m_treasuryId(std::move(treasuryId)),
      m_accountAddress(std::move(accountAddress)),
      m_balance(balance),
      m_epoch(epoch),
      m_locked(locked),
      m_lockReason(std::move(reason)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_treasuryId.empty()) {
        m_rejectionReason = "TreasuryAccount: treasuryId must not be empty.";
        return;
    }
    if (m_accountAddress.empty()) {
        m_rejectionReason = "TreasuryAccount: accountAddress must not be empty.";
        return;
    }
    if (m_balance.isNegative()) {
        m_rejectionReason = "TreasuryAccount: balance must not be negative.";
        return;
    }
    m_valid = true;
}

TreasuryAccount TreasuryAccount::invalid(std::string reason) {
    TreasuryAccount a;
    a.m_rejectionReason = std::move(reason);
    return a;
}

const std::string& TreasuryAccount::treasuryId() const { return m_treasuryId; }
const std::string& TreasuryAccount::accountAddress() const { return m_accountAddress; }
utils::Amount TreasuryAccount::balance() const { return m_balance; }
std::uint64_t TreasuryAccount::epoch() const { return m_epoch; }
bool TreasuryAccount::isLocked() const { return m_locked; }
const std::string& TreasuryAccount::lockReason() const { return m_lockReason; }
bool TreasuryAccount::isValid() const { return m_valid; }
const std::string& TreasuryAccount::rejectionReason() const { return m_rejectionReason; }

TreasuryAccount TreasuryAccount::credit(utils::Amount amount) const {
    if (!m_valid) {
        return TreasuryAccount::invalid("cannot credit invalid treasury account");
    }
    if (amount.isNegative() || amount.isZero()) {
        return TreasuryAccount::invalid("credit amount must be positive");
    }
    // Overflow guard.
    if (amount.rawUnits() > std::numeric_limits<std::int64_t>::max() - m_balance.rawUnits()) {
        return TreasuryAccount::invalid("credit would overflow treasury balance");
    }
    return TreasuryAccount(
        m_treasuryId,
        m_accountAddress,
        utils::Amount::fromRawUnits(m_balance.rawUnits() + amount.rawUnits()),
        m_epoch,
        m_locked,
        m_lockReason
    );
}

TreasuryDebitResult TreasuryAccount::tryDebit(utils::Amount amount) const {
    if (!amount.isPositive()) {
        return TreasuryDebitResult::negativeAmount();
    }
    if (m_locked) {
        return TreasuryDebitResult::locked();
    }
    if (amount > m_balance) {
        return TreasuryDebitResult::insufficientBalance(m_balance, amount);
    }
    return TreasuryDebitResult::accepted(
        utils::Amount::fromRawUnits(m_balance.rawUnits() - amount.rawUnits())
    );
}

std::string TreasuryAccount::serialize() const {
    std::ostringstream oss;
    oss << "TreasuryAccount{"
        << "id=" << m_treasuryId
        << ";address=" << m_accountAddress
        << ";balanceRaw=" << m_balance.rawUnits()
        << ";epoch=" << m_epoch
        << ";locked=" << (m_locked ? "1" : "0")
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
