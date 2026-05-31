#include "core/AccountState.hpp"
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

void testValidAccountState() {
    const nodo::core::AccountState account(
        "account-state-address",
        nodo::utils::Amount::fromRawUnits(1000),
        3
    );

    requireCondition(
        account.isValid() &&
        account.address() == "account-state-address" &&
        account.balance().rawUnits() == 1000 &&
        account.nonce() == 3,
        "Valid account state should expose address, balance and nonce."
    );
}

void testRejectsEmptyAddress() {
    const nodo::core::AccountState account(
        "",
        nodo::utils::Amount::fromRawUnits(1000),
        0
    );

    requireCondition(
        !account.isValid(),
        "AccountState should reject an empty address."
    );
}

void testRejectsNegativeBalance() {
    const nodo::core::AccountState account(
        "negative-balance-address",
        nodo::utils::Amount(-1),
        0
    );

    requireCondition(
        !account.isValid(),
        "AccountState should reject a negative balance."
    );
}

void testAcceptsNonceZero() {
    const nodo::core::AccountState account(
        "nonce-zero-address",
        nodo::utils::Amount::fromRawUnits(0),
        0
    );

    requireCondition(
        account.isValid(),
        "AccountState should accept nonce zero for new accounts."
    );
}

void testDeterministicSerialization() {
    const nodo::core::AccountState account(
        "serialize-address",
        nodo::utils::Amount::fromRawUnits(100),
        2
    );

    requireCondition(
        account.serialize() == "AccountState{address=serialize-address;balanceRaw=100;nonce=2}",
        "AccountState serialization should be deterministic."
    );
}

} // namespace

int main() {
    try {
        testValidAccountState();
        testRejectsEmptyAddress();
        testRejectsNegativeBalance();
        testAcceptsNonceZero();
        testDeterministicSerialization();

        std::cout << "Nodo account state tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo account state tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
