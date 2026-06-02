#include "economics/TreasuryAccount.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryDebitStatus;
using nodo::utils::Amount;

TreasuryAccount validAccount(
    Amount balance = Amount::fromRawUnits(10000),
    bool locked = false
) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr",
        balance, 0, locked, locked ? "locked for testing" : ""
    );
}

// tryDebit on an invalid (default-constructed) account returns INVALID_ACCOUNT.
void testTryDebitOnInvalidAccountRejected() {
    const TreasuryAccount invalid;
    assert(!invalid.isValid());

    const auto result = invalid.tryDebit(Amount::fromRawUnits(100));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::INVALID_ACCOUNT);
    assert(result.reason().find("invalid") != std::string::npos ||
           result.reason().find("account") != std::string::npos);
}

// tryDebit on an explicitly-invalid account returns INVALID_ACCOUNT.
void testTryDebitOnExplicitlyInvalidAccountRejected() {
    const TreasuryAccount invalid =
        TreasuryAccount::invalid("test: invalid for reasons");
    assert(!invalid.isValid());

    const auto result = invalid.tryDebit(Amount::fromRawUnits(500));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::INVALID_ACCOUNT);
}

// tryDebit on a valid unlocked account succeeds.
void testTryDebitOnValidAccountAccepted() {
    const auto result = validAccount().tryDebit(Amount::fromRawUnits(5000));
    assert(result.accepted());
    assert(result.status() == TreasuryDebitStatus::ACCEPTED);
    assert(result.balanceAfter() == Amount::fromRawUnits(5000));
}

// tryDebit on a locked account returns LOCKED (not INVALID_ACCOUNT).
void testTryDebitOnLockedAccountReturnedLocked() {
    const auto result = validAccount(Amount::fromRawUnits(10000), true)
                            .tryDebit(Amount::fromRawUnits(100));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::LOCKED);
}

// tryDebit with zero amount returns NEGATIVE_AMOUNT.
void testTryDebitZeroAmountRejected() {
    const auto result = validAccount().tryDebit(Amount::fromRawUnits(0));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::NEGATIVE_AMOUNT);
}

// tryDebit above balance returns INSUFFICIENT_BALANCE.
void testTryDebitAboveBalanceRejected() {
    const auto result = validAccount(Amount::fromRawUnits(100))
                            .tryDebit(Amount::fromRawUnits(101));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::INSUFFICIENT_BALANCE);
}

} // namespace

int main() {
    testTryDebitOnInvalidAccountRejected();
    testTryDebitOnExplicitlyInvalidAccountRejected();
    testTryDebitOnValidAccountAccepted();
    testTryDebitOnLockedAccountReturnedLocked();
    testTryDebitZeroAmountRejected();
    testTryDebitAboveBalanceRejected();
    return 0;
}
