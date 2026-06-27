#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/StateRootCalculator.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <iostream>
#include <map>

int main() {
    nodo::core::AccountStateView accounts;
    assert(accounts.putAccount(nodo::core::AccountState(
        "protocol-state-account",
        nodo::utils::Amount::fromRawUnits(1000),
        3
    )));

    const std::map<std::string, std::string> domainsA = {
        {"governance", "Governance{version=1}"},
        {"supply", "RuntimeSupply{latestRawUnits=1000}"},
        {"validators", "ValidatorRegistry{size=1}"}
    };
    const std::map<std::string, std::string> domainsB = {
        {"validators", "ValidatorRegistry{size=1}"},
        {"supply", "RuntimeSupply{latestRawUnits=1000}"},
        {"governance", "Governance{version=1}"}
    };

    const std::string rootA =
        nodo::core::StateRootCalculator::calculateProtocolStateRoot(accounts, domainsA);
    const std::string rootB =
        nodo::core::StateRootCalculator::calculateProtocolStateRoot(accounts, domainsB);
    assert(!rootA.empty());
    assert(rootA == rootB);

    auto changedDomains = domainsA;
    changedDomains["supply"] = "RuntimeSupply{latestRawUnits=999}";
    assert(rootA != nodo::core::StateRootCalculator::calculateProtocolStateRoot(
        accounts, changedDomains
    ));

    std::cout << "protocol state root tests passed\n";
    return 0;
}
