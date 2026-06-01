#include "core/CoinLot.hpp"
#include "core/CoinLotRegistry.hpp"
#include "core/CoinLotTransactionValidator.hpp"
#include "core/CoinLotTransferPlan.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::CoinLot;
using nodo::core::CoinLotRegistry;
using nodo::core::CoinLotTransactionValidator;
using nodo::core::CoinLotTransferPlan;
using nodo::core::CoinLotStatus;
using nodo::core::State;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::economics::MintReason;
using nodo::economics::MintRecord;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

CoinLot makeLot(
    const std::string& id,
    const std::string& origin,
    const std::string& owner,
    std::int64_t wholeNodo,
    CoinLotStatus status = CoinLotStatus::AVAILABLE
) {
    return CoinLot(
        id,
        origin,
        owner,
        Amount::fromNodo(wholeNodo),
        status,
        1,
        0,
        kTimestamp
    );
}

Transaction makeTransfer(
    const std::string& from,
    const std::string& to,
    std::int64_t amount,
    std::int64_t fee,
    std::uint64_t nonce
) {
    return Transaction(
        TransactionType::TRANSFER,
        from,
        to,
        Amount::fromNodo(amount),
        Amount::fromNodo(fee),
        nonce,
        kTimestamp
    );
}

MintRecord makeMint(
    const std::string& id,
    const std::string& recipient,
    std::int64_t amount
) {
    return MintRecord(
        id,
        "auth_coinlot_test_001",
        recipient,
        Amount::fromNodo(amount),
        MintReason::GENESIS_ALLOCATION,
        1,
        0,
        "genesis-hash",
        kTimestamp
    );
}

void testValidatorBuildsDeterministicTransferPlan() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "lot-a",
            "origin-a",
            "nodo1igor",
            50
        )
    );

    registry.addLot(
        makeLot(
            "lot-b",
            "origin-b",
            "nodo1igor",
            20
        )
    );

    const Transaction transaction =
        makeTransfer(
            "nodo1igor",
            "nodo1ana",
            55,
            5,
            1
        );

    const CoinLotTransferPlan plan =
        CoinLotTransactionValidator::buildTransferPlan(
            transaction,
            registry,
            2
        );

    requireCondition(
        plan.isValid(),
        "Generated transfer plan should be valid."
    );

    requireCondition(
        plan.inputLotIds().size() == 2U,
        "Transfer plan should use two input lots."
    );

    requireCondition(
        plan.outputLots().size() == 4U,
        "Transfer plan should create recipient, fee and change outputs."
    );

    requireCondition(
        plan.transferAmount() == Amount::fromNodo(55),
        "Transfer plan amount is wrong."
    );

    requireCondition(
        plan.feeAmount() == Amount::fromNodo(5),
        "Transfer plan fee is wrong."
    );

    requireCondition(
        plan.changeAmount() == Amount::fromNodo(10),
        "Transfer plan change is wrong."
    );

    requireCondition(
        plan.totalInputAmount() == Amount::fromNodo(70),
        "Transfer plan total input amount is wrong."
    );

    requireCondition(
        plan.totalOutputAmount() == Amount::fromNodo(70),
        "Transfer plan total output amount is wrong."
    );
}

void testValidatorAppliesTransferToRegistry() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "lot-a",
            "origin-a",
            "nodo1igor",
            50
        )
    );

    registry.addLot(
        makeLot(
            "lot-b",
            "origin-b",
            "nodo1igor",
            20
        )
    );

    const Transaction transaction =
        makeTransfer(
            "nodo1igor",
            "nodo1ana",
            55,
            5,
            1
        );

    CoinLotTransactionValidator::applyTransfer(
        registry,
        transaction,
        2
    );

    requireCondition(
        registry.lot("lot-a").isSpent(),
        "First input lot should be spent."
    );

    requireCondition(
        registry.lot("lot-b").isSpent(),
        "Second input lot should be spent."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1ana") == Amount::fromNodo(55),
        "Recipient balance is wrong after registry transfer."
    );

    requireCondition(
        registry.availableBalanceForOwner(State::feePoolAddress()) == Amount::fromNodo(5),
        "Fee pool balance is wrong after registry transfer."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1igor") == Amount::fromNodo(10),
        "Change balance is wrong after registry transfer."
    );

    requireCondition(
        registry.totalAvailableAmount() == Amount::fromNodo(70),
        "Available amount should be preserved in outputs."
    );

    requireCondition(
        registry.totalSpentAmount() == Amount::fromNodo(70),
        "Spent amount should equal consumed inputs."
    );
}

void testValidatorRejectsLockedSpentAndSlashedLots() {
    CoinLotRegistry registry;

    CoinLot locked =
        makeLot(
            "locked-lot",
            "origin-locked",
            "nodo1igor",
            100
        );

    locked.lockForSecurity(20);

    registry.addLot(locked);

    registry.addLot(
        makeLot(
            "spent-lot",
            "origin-spent",
            "nodo1igor",
            50,
            CoinLotStatus::SPENT
        )
    );

    registry.addLot(
        makeLot(
            "slashed-lot",
            "origin-slashed",
            "nodo1igor",
            50,
            CoinLotStatus::SLASHED
        )
    );

    const Transaction transaction =
        makeTransfer(
            "nodo1igor",
            "nodo1ana",
            10,
            1,
            1
        );

    requireCondition(
        !CoinLotTransactionValidator::validateTransferTransaction(
            transaction,
            registry
        ).success(),
        "Locked, spent and slashed lots must not be accepted as spendable."
    );
}

void testStateAppliesTransferThroughCoinLotRegistry() {
    State state;

    state.applyMintRecord(
        makeMint(
            "mint-1",
            "nodo1igor",
            100
        )
    );

    const Transaction transaction =
        makeTransfer(
            "nodo1igor",
            "nodo1ana",
            30,
            2,
            1
        );

    state.applyTransferTransactionUsingRegistry(transaction);

    requireCondition(
        state.balanceOf("nodo1igor") == Amount::fromNodo(68),
        "Sender balance is wrong after State registry transfer."
    );

    requireCondition(
        state.balanceOf("nodo1ana") == Amount::fromNodo(30),
        "Recipient balance is wrong after State registry transfer."
    );

    requireCondition(
        state.balanceOf(State::feePoolAddress()) == Amount::fromNodo(2),
        "Fee pool balance is wrong after State registry transfer."
    );

    requireCondition(
        state.nextNonceOf("nodo1igor") == 2,
        "Sender nonce was not advanced."
    );

    requireCondition(
        state.isTransactionAlreadyApplied(transaction.id()),
        "Applied transaction id was not recorded."
    );

    requireCondition(
        state.isSupplyAuditable(),
        "State supply should remain auditable after registry transfer."
    );

    bool duplicateRejected = false;

    try {
        state.applyTransferTransactionUsingRegistry(transaction);
    } catch (const std::exception&) {
        duplicateRejected = true;
    }

    requireCondition(
        duplicateRejected,
        "Duplicate transaction should be rejected."
    );
}

void testStateRejectsInsufficientRegistryBackedBalance() {
    State state;

    state.applyMintRecord(
        makeMint(
            "mint-1",
            "nodo1igor",
            10
        )
    );

    const Transaction transaction =
        makeTransfer(
            "nodo1igor",
            "nodo1ana",
            20,
            1,
            1
        );

    bool rejected = false;

    try {
        state.applyTransferTransactionUsingRegistry(transaction);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "State accepted transfer without enough spendable CoinLots."
    );

    requireCondition(
        state.balanceOf("nodo1igor") == Amount::fromNodo(10),
        "Sender balance changed after rejected transfer."
    );

    requireCondition(
        state.isSupplyAuditable(),
        "State supply should remain auditable after rejected transfer."
    );
}

} // namespace

int main() {
    try {
        testValidatorBuildsDeterministicTransferPlan();
        testValidatorAppliesTransferToRegistry();
        testValidatorRejectsLockedSpentAndSlashedLots();
        testStateAppliesTransferThroughCoinLotRegistry();
        testStateRejectsInsufficientRegistryBackedBalance();

        std::cout << "Nodo coin lot transaction integration tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo coin lot transaction integration tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
