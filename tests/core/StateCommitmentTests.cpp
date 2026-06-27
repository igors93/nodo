#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/StateCommitment.hpp"
#include "core/ValidatorRegistry.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string BLOCK_HASH =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

nodo::core::AccountStateView makeAccountsInOriginalOrder() {
    nodo::core::AccountStateView view;
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-b",
            nodo::utils::Amount::fromRawUnits(200),
            2
        )
    ));
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-a",
            nodo::utils::Amount::fromRawUnits(100),
            1
        )
    ));
    return view;
}

nodo::core::AccountStateView makeAccountsInDifferentOrder() {
    nodo::core::AccountStateView view;
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-a",
            nodo::utils::Amount::fromRawUnits(100),
            1
        )
    ));
    assert(view.putAccount(
        nodo::core::AccountState(
            "account-b",
            nodo::utils::Amount::fromRawUnits(200),
            2
        )
    ));
    return view;
}

} // namespace

int main() {
    nodo::core::ValidatorRegistry registry;
    const std::vector<nodo::core::LedgerRecord> ledgerRecords;

    const nodo::core::StateCommitment first =
        nodo::core::StateCommitment::calculate(
            10,
            BLOCK_HASH,
            makeAccountsInOriginalOrder(),
            ledgerRecords,
            registry,
            1700000000
        );

    const nodo::core::StateCommitment second =
        nodo::core::StateCommitment::calculate(
            10,
            BLOCK_HASH,
            makeAccountsInDifferentOrder(),
            ledgerRecords,
            registry,
            1700000000
        );

    assert(first.isValid());
    assert(second.isValid());
    assert(first.accountRoot() == second.accountRoot());
    assert(first.ledgerRoot() == second.ledgerRoot());
    assert(first.validatorRoot() == second.validatorRoot());
    assert(first.finalizedStateRoot() == second.finalizedStateRoot());
    assert(first.serialize().find("StateCommitment") != std::string::npos);

    const nodo::core::StateCommitment changedHeight =
        nodo::core::StateCommitment::calculate(
            11,
            BLOCK_HASH,
            makeAccountsInOriginalOrder(),
            ledgerRecords,
            registry,
            1700000000
        );

    assert(changedHeight.finalizedStateRoot() != first.finalizedStateRoot());

    return 0;
}
