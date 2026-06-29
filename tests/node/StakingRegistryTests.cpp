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
    assert(reg.isValid());
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
    assert(s.find("positionCount=1") != std::string::npos);
}

void testAccountsMapIsOrdered() {
    StakingRegistry reg;
    reg.setAccount("val-z", StakeAccount("val-z", Amount::fromRawUnits(1)));
    reg.setAccount("val-a", StakeAccount("val-a", Amount::fromRawUnits(2)));
    const auto& map = reg.accounts();
    assert(map.begin()->first == "val-a");
}

void testDepositActivationUnlockWithdrawLifecycle() {
    StakingRegistry reg;
    reg.deposit("owner-a", "val-a", Amount::fromRawUnits(1'000), 10, false, "tx-deposit");
    assert(reg.pendingActivationStake("owner-a", "val-a").rawUnits() == 1'000);
    assert(reg.activeStake("owner-a", "val-a").isZero());
    assert(reg.activatePending(11));
    assert(reg.activeStake("owner-a", "val-a").rawUnits() == 1'000);

    reg.requestUnlock("owner-a", "val-a", Amount::fromRawUnits(400), 12, "tx-unlock");
    assert(reg.activeStake("owner-a", "val-a").rawUnits() == 600);
    assert(reg.pendingUnbondingStake("owner-a", "val-a").rawUnits() == 400);
    assert(reg.withdrawableStake("owner-a", "val-a", 12).isZero());
    assert(reg.withdrawableStake("owner-a", "val-a", 33).rawUnits() == 400);

    reg.withdraw("owner-a", "val-a", Amount::fromRawUnits(400), 33, "tx-withdraw");
    assert(reg.ownedStake("owner-a", "val-a").rawUnits() == 600);
    assert(reg.accountOrDefault("val-a").bondedAmount().rawUnits() == 600);
    assert(reg.lifecycleRecords().size() == 4);
    assert(reg.isValid());
}

void testPenaltyStateSlashesActiveStakeAndBlocksWithdrawableAmount() {
    StakingRegistry reg;
    reg.deposit("owner-a", "val-a", Amount::fromRawUnits(1'000), 10, false);
    reg.activatePending(11);
    reg.applyPenaltyState("val-a", Amount::fromRawUnits(250), true, false, 12);

    const auto account = reg.accountOrDefault("val-a");
    assert(account.bondedAmount().rawUnits() == 1'000);
    assert(account.slashedAmount().rawUnits() == 250);
    assert(account.jailed());
    assert(reg.activeStakeFor("val-a").isZero());
    assert(reg.activeStake("owner-a", "val-a").rawUnits() == 750);
    assert(reg.isValid());
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
    testDepositActivationUnlockWithdrawLifecycle();
    testPenaltyStateSlashesActiveStakeAndBlocksWithdrawableAmount();
    return 0;
}
