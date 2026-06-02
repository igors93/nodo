#include "economics/TreasuryAccount.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryDebitStatus;
using nodo::utils::Amount;

TreasuryAccount validAccount(
    Amount balance = Amount::fromRawUnits(1000000)
) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-address",
        balance, 0, false, ""
    );
}

// Valid treasury account.
void testValidTreasuryAccount() {
    const auto a = validAccount();
    assert(a.isValid());
    assert(a.treasuryId() == "treasury-main");
    assert(a.accountAddress() == "nodo-treasury-address");
    assert(!a.isLocked());
}

// Empty treasuryId is rejected.
void testEmptyIdRejected() {
    const TreasuryAccount a("", "addr", Amount::fromRawUnits(1000), 0, false, "");
    assert(!a.isValid());
    assert(a.rejectionReason().find("treasuryId") != std::string::npos);
}

// Empty accountAddress is rejected.
void testEmptyAddressRejected() {
    const TreasuryAccount a("id", "", Amount::fromRawUnits(1000), 0, false, "");
    assert(!a.isValid());
    assert(a.rejectionReason().find("accountAddress") != std::string::npos);
}

// Negative balance is rejected. Amount itself throws for negative raw units,
// which is the primary defense — the system cannot create a negative Amount.
void testNegativeBalanceRejected() {
    bool threw = false;
    try {
        const TreasuryAccount a("id", "addr", Amount::fromRawUnits(-1), 0, false, "");
        (void)a;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Credit increases balance.
void testCreditIncreasesBalance() {
    const auto a = validAccount(Amount::fromRawUnits(1000));
    const auto credited = a.credit(Amount::fromRawUnits(500));
    assert(credited.isValid());
    assert(credited.balance() == Amount::fromRawUnits(1500));
}

// Credit overflow is rejected.
void testCreditOverflowRejected() {
    const auto a = validAccount(Amount::fromRawUnits(
        std::numeric_limits<std::int64_t>::max() - 10
    ));
    const auto credited = a.credit(Amount::fromRawUnits(100));
    assert(!credited.isValid());
}

// Debit decreases balance.
void testDebitDecreasesBalance() {
    const auto a = validAccount(Amount::fromRawUnits(1000));
    const auto result = a.tryDebit(Amount::fromRawUnits(300));
    assert(result.accepted());
    assert(result.balanceAfter() == Amount::fromRawUnits(700));
}

// Debit above balance is rejected.
void testDebitAboveBalanceRejected() {
    const auto a = validAccount(Amount::fromRawUnits(100));
    const auto result = a.tryDebit(Amount::fromRawUnits(200));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::INSUFFICIENT_BALANCE);
}

// Locked treasury rejects debit.
void testLockedTreasuryRejectsDebit() {
    const TreasuryAccount locked(
        "locked-treasury", "addr", Amount::fromRawUnits(5000), 0, true, "under investigation"
    );
    assert(locked.isValid());
    const auto result = locked.tryDebit(Amount::fromRawUnits(100));
    assert(!result.accepted());
    assert(result.status() == TreasuryDebitStatus::LOCKED);
}

} // namespace

int main() {
    testValidTreasuryAccount();
    testEmptyIdRejected();
    testEmptyAddressRejected();
    testNegativeBalanceRejected();
    testCreditIncreasesBalance();
    testCreditOverflowRejected();
    testDebitDecreasesBalance();
    testDebitAboveBalanceRejected();
    testLockedTreasuryRejectsDebit();
    return 0;
}
