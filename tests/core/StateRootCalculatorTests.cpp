#include "core/StateRootCalculator.hpp"

#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

nodo::core::AccountState account(
    const std::string& address,
    std::int64_t balance,
    std::uint64_t nonce
) {
    return nodo::core::AccountState(
        address,
        nodo::utils::Amount::fromRawUnits(balance),
        nonce
    );
}

void testOrderIndependentRoot() {
    nodo::core::AccountStateView first;
    first.putAccount(account("b", 2, 0));
    first.putAccount(account("a", 1, 0));

    nodo::core::AccountStateView second;
    second.putAccount(account("a", 1, 0));
    second.putAccount(account("b", 2, 0));

    requireCondition(
        nodo::core::StateRootCalculator::calculateAccountStateRoot(first) ==
        nodo::core::StateRootCalculator::calculateAccountStateRoot(second),
        "Account state root should not depend on insertion order."
    );
}

void testBalanceChangesRoot() {
    nodo::core::AccountStateView first;
    first.putAccount(account("a", 1, 0));

    nodo::core::AccountStateView second;
    second.putAccount(account("a", 2, 0));

    requireCondition(
        nodo::core::StateRootCalculator::calculateAccountStateRoot(first) !=
        nodo::core::StateRootCalculator::calculateAccountStateRoot(second),
        "Balance changes should change account state root."
    );
}

void testNonceChangesRoot() {
    nodo::core::AccountStateView first;
    first.putAccount(account("a", 1, 0));

    nodo::core::AccountStateView second;
    second.putAccount(account("a", 1, 1));

    requireCondition(
        nodo::core::StateRootCalculator::calculateAccountStateRoot(first) !=
        nodo::core::StateRootCalculator::calculateAccountStateRoot(second),
        "Nonce changes should change account state root."
    );
}

void testExtraAccountChangesRoot() {
    nodo::core::AccountStateView first;
    first.putAccount(account("a", 1, 0));

    nodo::core::AccountStateView second;
    second.putAccount(account("a", 1, 0));
    second.putAccount(account("b", 0, 0));

    requireCondition(
        nodo::core::StateRootCalculator::calculateAccountStateRoot(first) !=
        nodo::core::StateRootCalculator::calculateAccountStateRoot(second),
        "Extra account should change account state root."
    );
}

void testEmptyRootIsExplicit() {
    const nodo::core::AccountStateView view;

    requireCondition(
        !nodo::core::StateRootCalculator::calculateAccountStateRoot(view).empty(),
        "Empty account state should have an explicit deterministic root."
    );
}

} // namespace

int main() {
    try {
        testOrderIndependentRoot();
        testBalanceChangesRoot();
        testNonceChangesRoot();
        testExtraAccountChangesRoot();
        testEmptyRootIsExplicit();

        std::cout << "Nodo state root calculator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo state root calculator tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
