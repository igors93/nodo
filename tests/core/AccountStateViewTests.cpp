#include "core/AccountStateView.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

nodo::core::AccountState validAccount(
    const std::string& address
) {
    return nodo::core::AccountState(
        address,
        nodo::utils::Amount::fromRawUnits(1000),
        1
    );
}

void testInsertsValidAccount() {
    nodo::core::AccountStateView view;

    requireCondition(
        view.putAccount(validAccount("state-view-account")) &&
        view.hasAccount("state-view-account") &&
        view.isValid(),
        "AccountStateView should insert valid accounts."
    );
}

void testRejectsInvalidAccount() {
    nodo::core::AccountStateView view;

    requireCondition(
        !view.putAccount(
            nodo::core::AccountState(
                "",
                nodo::utils::Amount::fromRawUnits(1),
                0
            )
        ) &&
        view.knownAddresses().empty() &&
        view.isValid(),
        "AccountStateView should reject invalid accounts without mutating state."
    );
}

void testConsultsExistingAccount() {
    nodo::core::AccountStateView view;
    view.putAccount(validAccount("known-account"));

    const nodo::core::AccountState account =
        view.accountOrDefault("known-account");

    requireCondition(
        account.address() == "known-account" &&
        account.balance().rawUnits() == 1000 &&
        account.nonce() == 1,
        "AccountStateView should return existing account state."
    );
}

void testConsultsMissingAccountExplicitly() {
    const nodo::core::AccountStateView view;

    const nodo::core::AccountState account =
        view.accountOrDefault("missing-account");

    requireCondition(
        !view.hasAccount("missing-account") &&
        account.address() == "missing-account" &&
        account.balance().rawUnits() == 0 &&
        account.nonce() == 0 &&
        account.isValid(),
        "AccountStateView should return explicit zero state for missing accounts."
    );
}

void testDeterministicAddressListing() {
    nodo::core::AccountStateView view;
    view.putAccount(validAccount("b-account"));
    view.putAccount(validAccount("a-account"));

    const std::vector<std::string> addresses =
        view.knownAddresses();

    requireCondition(
        addresses.size() == 2 &&
        addresses[0] == "a-account" &&
        addresses[1] == "b-account",
        "AccountStateView should list addresses deterministically."
    );
}

} // namespace

int main() {
    try {
        testInsertsValidAccount();
        testRejectsInvalidAccount();
        testConsultsExistingAccount();
        testConsultsMissingAccountExplicitly();
        testDeterministicAddressListing();

        std::cout << "Nodo account state view tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo account state view tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
