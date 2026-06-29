#include "node/StakingRegistry.hpp"

#include "economics/StakeAccount.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::StakingRegistry;
using nodo::economics::StakeAccount;
using nodo::utils::Amount;

void testDefaultIsEmpty() {
    const StakingRegistry reg;
    assert(reg.size() == 0);
    assert(reg.isValid());
}

void testHasAccount() {
    StakingRegistry reg;
    assert(!reg.hasAccount("validator-a"));
    reg.setAccount("validator-a", StakeAccount("validator-a", Amount::fromRawUnits(1'000'000)));
    assert(reg.hasAccount("validator-a"));
    assert(!reg.hasAccount("validator-b"));
}

void testAccountOrDefaultReturnsZeroBond() {
    const StakingRegistry reg;
    const auto acc = reg.accountOrDefault("unknown");
    assert(acc.validatorAddress() == "unknown");
    assert(acc.bondedAmount().isZero());
}

void testSetAndGetAccount() {
    StakingRegistry reg;
    StakeAccount acc("val-1", Amount::fromRawUnits(5'000'000));
    reg.setAccount("val-1", acc);

    const auto* ptr = reg.accountFor("val-1");
    assert(ptr != nullptr);
    assert(ptr->bondedAmount().rawUnits() == 5'000'000);
}

void testSetOverwritesExistingAccount() {
    StakingRegistry reg;
    reg.setAccount("val-1", StakeAccount("val-1", Amount::fromRawUnits(1'000'000)));
    reg.setAccount("val-1", StakeAccount("val-1", Amount::fromRawUnits(3'000'000)));
    assert(reg.size() == 1);
    assert(reg.accountFor("val-1")->bondedAmount().rawUnits() == 3'000'000);
}

void testMultipleValidators() {
    StakingRegistry reg;
    reg.setAccount("val-a", StakeAccount("val-a", Amount::fromRawUnits(1'000'000)));
    reg.setAccount("val-b", StakeAccount("val-b", Amount::fromRawUnits(2'000'000)));
    reg.setAccount("val-c", StakeAccount("val-c", Amount::fromRawUnits(3'000'000)));
    assert(reg.size() == 3);
    assert(reg.hasAccount("val-a"));
    assert(reg.hasAccount("val-b"));
    assert(reg.hasAccount("val-c"));
}

void testIsValid() {
    StakingRegistry reg;
    assert(reg.isValid());
    reg.setAccount("val-x", StakeAccount("val-x", Amount::fromRawUnits(100)));
    assert(reg.isValid());
}

void testSerializeNonEmpty() {
    StakingRegistry reg;
    reg.setAccount("val-1", StakeAccount("val-1", Amount::fromRawUnits(1'000'000)));
    const std::string s = reg.serialize();
    assert(!s.empty());
    assert(s.find("count=1") != std::string::npos);
}

void testAccountsMapIsOrdered() {
    StakingRegistry reg;
    reg.setAccount("val-z", StakeAccount("val-z", Amount::fromRawUnits(1)));
    reg.setAccount("val-a", StakeAccount("val-a", Amount::fromRawUnits(2)));
    const auto& map = reg.accounts();
    assert(map.begin()->first == "val-a");
}

} // namespace

int main() {
    testDefaultIsEmpty();
    testHasAccount();
    testAccountOrDefaultReturnsZeroBond();
    testSetAndGetAccount();
    testSetOverwritesExistingAccount();
    testMultipleValidators();
    testIsValid();
    testSerializeNonEmpty();
    testAccountsMapIsOrdered();
    return 0;
}
