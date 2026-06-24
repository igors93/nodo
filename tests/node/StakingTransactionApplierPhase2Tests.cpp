#include "node/StakingTransactionApplier.hpp"

#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "economics/StakeAccount.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::economics::StakeAccount;
using nodo::node::StakingApplyStatus;
using nodo::node::StakingTransactionApplier;
using nodo::utils::Amount;

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

Transaction makeTx(
    TransactionType type,
    const std::string& from,
    const std::string& to,
    Amount amount,
    Amount fee = Amount::fromRawUnits(100),
    std::uint64_t nonce = 1,
    std::int64_t ts = 1900000000
) {
    return Transaction(type, from, to, amount, fee, nonce, ts);
}

// ── 1. STAKE_DEPOSIT with sufficient balance passes ──────────────────────────

void testStakeDepositAccepted() {
    StakeAccount stake("val-deposit", Amount());
    const Amount balance = Amount::fromRawUnits(5'000'000);
    const Amount depositAmt = Amount::fromRawUnits(2'000'000);

    const auto tx = makeTx(
        TransactionType::STAKE_DEPOSIT,
        "val-deposit", "val-deposit",
        depositAmt
    );

    const auto result = StakingTransactionApplier::apply(
        tx, stake, balance, 100
    );

    requireCondition(result.applied(), "STAKE_DEPOSIT with sufficient balance must apply");
    requireCondition(
        result.updatedStake().bondedAmount() == depositAmt,
        "Bonded amount must equal deposit amount"
    );
}

// ── 2. STAKE_DEPOSIT below minimum stake rejected ────────────────────────────

void testStakeDepositBelowMinimumRejected() {
    StakeAccount stake("val-min", Amount());
    const Amount balance  = Amount::fromRawUnits(10'000'000);
    const Amount tooSmall = Amount::fromRawUnits(500'000);  // below 1'000'000

    const auto tx = makeTx(
        TransactionType::STAKE_DEPOSIT,
        "val-min", "val-min",
        tooSmall
    );

    const Amount minStake = Amount::fromRawUnits(1'000'000);
    const auto result = StakingTransactionApplier::apply(
        tx, stake, balance, 100, minStake
    );

    requireCondition(
        result.status() == StakingApplyStatus::BELOW_MINIMUM_STAKE,
        "STAKE_DEPOSIT below minimum stake must be rejected"
    );
}

// ── 3. STAKE_DEPOSIT with insufficient balance rejected ──────────────────────

void testStakeDepositInsufficientBalance() {
    StakeAccount stake("val-insuf", Amount());
    const Amount balance = Amount::fromRawUnits(500'000);
    const Amount deposit = Amount::fromRawUnits(1'000'000);

    const auto tx = makeTx(
        TransactionType::STAKE_DEPOSIT,
        "val-insuf", "val-insuf",
        deposit
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, balance, 100);
    requireCondition(
        result.status() == StakingApplyStatus::INSUFFICIENT_BALANCE,
        "STAKE_DEPOSIT with insufficient balance must be rejected"
    );
}

// ── 4. STAKE_TOP_UP is treated like a deposit ────────────────────────────────

void testStakeTopUpAccepted() {
    StakeAccount stake("val-topup", Amount::fromRawUnits(1'000'000));
    const Amount balance = Amount::fromRawUnits(5'000'000);
    const Amount topUp   = Amount::fromRawUnits(500'000);

    const auto tx = makeTx(
        TransactionType::STAKE_TOP_UP,
        "val-topup", "val-topup",
        topUp
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, balance, 100);
    requireCondition(result.applied(), "STAKE_TOP_UP with sufficient balance must apply");
    requireCondition(
        result.updatedStake().bondedAmount() ==
            Amount::fromRawUnits(1'000'000 + 500'000),
        "STAKE_TOP_UP must add to existing bonded amount"
    );
}

// ── 5. STAKE_WITHDRAW when jailed is rejected ────────────────────────────────

void testStakeWithdrawWhileJailedRejected() {
    StakeAccount stake("val-jailed", Amount::fromRawUnits(3'000'000));
    stake.jail();

    const auto tx = makeTx(
        TransactionType::STAKE_WITHDRAW,
        "val-jailed", "val-jailed",
        Amount::fromRawUnits(1'000'000)
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, Amount(), 100);
    requireCondition(
        result.status() == StakingApplyStatus::VALIDATOR_JAILED,
        "STAKE_WITHDRAW while jailed must be rejected"
    );
}

// ── 6. STAKE_WITHDRAW when tombstoned is rejected ────────────────────────────

void testStakeWithdrawWhileTombstonedRejected() {
    StakeAccount stake("val-tomb", Amount::fromRawUnits(3'000'000));
    stake.tombstone();

    const auto tx = makeTx(
        TransactionType::STAKE_WITHDRAW,
        "val-tomb", "val-tomb",
        Amount::fromRawUnits(1'000'000)
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, Amount(), 200);
    requireCondition(
        result.status() == StakingApplyStatus::VALIDATOR_TOMBSTONED,
        "STAKE_WITHDRAW while tombstoned must be rejected"
    );
}

// ── 7. STAKE_WITHDRAW with insufficient bonded amount ────────────────────────

void testStakeWithdrawInsufficientStake() {
    StakeAccount stake("val-insuf-stake", Amount::fromRawUnits(100'000));

    const auto tx = makeTx(
        TransactionType::STAKE_WITHDRAW,
        "val-insuf-stake", "val-insuf-stake",
        Amount::fromRawUnits(500'000)
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, Amount(), 100);
    requireCondition(
        result.status() == StakingApplyStatus::INSUFFICIENT_STAKE,
        "STAKE_WITHDRAW exceeding bonded amount must be rejected"
    );
}

// ── 8. VALIDATOR_EXIT_REQUEST from eligible validator applies ─────────────────

void testValidatorExitRequestApplied() {
    StakeAccount stake("val-exit", Amount::fromRawUnits(2'000'000));
    const Amount fee = Amount::fromRawUnits(100);

    const auto tx = makeTx(
        TransactionType::VALIDATOR_EXIT_REQUEST,
        "val-exit", "val-exit",
        Amount(),  // zero amount for exit request
        fee
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, Amount::fromRawUnits(1'000), 100);
    requireCondition(
        result.applied(),
        "VALIDATOR_EXIT_REQUEST from eligible validator must apply"
    );
    // Stake account is unchanged (exit only records intent; fee is deducted by caller)
    requireCondition(
        result.updatedStake().bondedAmount() == Amount::fromRawUnits(2'000'000),
        "EXIT_REQUEST must not change bonded amount"
    );
}

// ── 9. VALIDATOR_EXIT_REQUEST while tombstoned is rejected ───────────────────

void testValidatorExitRequestTombstoned() {
    StakeAccount stake("val-tomb-exit", Amount::fromRawUnits(2'000'000));
    stake.tombstone();
    const Amount fee = Amount::fromRawUnits(100);

    const auto tx = makeTx(
        TransactionType::VALIDATOR_EXIT_REQUEST,
        "val-tomb-exit", "val-tomb-exit",
        Amount(), fee
    );

    const auto result = StakingTransactionApplier::apply(
        tx, stake, Amount::fromRawUnits(1'000), 100
    );
    requireCondition(
        result.status() == StakingApplyStatus::VALIDATOR_TOMBSTONED,
        "VALIDATOR_EXIT_REQUEST while tombstoned must be rejected"
    );
}

// ── 10. Non-staking transaction returns INVALID_TRANSACTION ──────────────────

void testNonStakingTransactionRejected() {
    StakeAccount stake("val-nonstaking", Amount::fromRawUnits(1'000'000));

    const auto tx = makeTx(
        TransactionType::TRANSFER,
        "val-nonstaking", "recipient",
        Amount::fromRawUnits(100)
    );

    const auto result = StakingTransactionApplier::apply(tx, stake, Amount::fromRawUnits(1'000), 100);
    requireCondition(
        result.status() == StakingApplyStatus::INVALID_TRANSACTION,
        "Non-staking transaction must be rejected as INVALID_TRANSACTION"
    );
}

} // namespace

int main() {
    try {
        testStakeDepositAccepted();
        testStakeDepositBelowMinimumRejected();
        testStakeDepositInsufficientBalance();
        testStakeTopUpAccepted();
        testStakeWithdrawWhileJailedRejected();
        testStakeWithdrawWhileTombstonedRejected();
        testStakeWithdrawInsufficientStake();
        testValidatorExitRequestApplied();
        testValidatorExitRequestTombstoned();
        testNonStakingTransactionRejected();

        std::cout << "Nodo StakingTransactionApplier Phase 2 tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo StakingTransactionApplier Phase 2 tests FAILED: "
                  << e.what() << "\n";
        return 1;
    }
}
