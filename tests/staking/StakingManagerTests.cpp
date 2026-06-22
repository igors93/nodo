#include "staking/StakingManager.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::staking::StakingManager;

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testStakingBondAndSelection() {
    StakingManager manager(10); // 10 blocks unbonding period

    requireCondition(manager.bondStake("val1", 1000), "Should bond val1");
    requireCondition(manager.bondStake("val2", 2000), "Should bond val2");
    requireCondition(manager.getValidatorStake("val1") == 1000, "Val1 stake should be 1000");
    requireCondition(manager.getValidatorStake("val2") == 2000, "Val2 stake should be 2000");

    auto activeSet = manager.getActiveValidatorSet(2);
    requireCondition(activeSet.size() == 2, "Active set size should be 2");
    requireCondition(activeSet[0] == "val2", "Val2 should be first (highest stake)");
    requireCondition(activeSet[1] == "val1", "Val1 should be second");
}

void testStakingUnbonding() {
    StakingManager manager(10);
    manager.bondStake("val1", 1000);

    requireCondition(manager.initiateUnbond("val1", 400, 100), "Initiate unbond of 400 at block 100");
    requireCondition(manager.getValidatorStake("val1") == 600, "Val1 stake should reduce to 600");

    // Process unbonding at block 105 (not unlockable yet)
    manager.processUnbondingQueue(105);

    // Process unbonding at block 110 (unlockable!)
    manager.processUnbondingQueue(110);
}

void testJailingAndSlashing() {
    StakingManager manager(10);
    manager.bondStake("val1", 1000);
    manager.bondStake("val2", 500);

    // Jail val1 at block 100 for 50 blocks
    manager.jailValidator("val1", 50, 100);
    requireCondition(manager.isJailed("val1", 120), "Val1 should be jailed at block 120");
    requireCondition(!manager.isJailed("val1", 160), "Val1 should not be jailed at block 160");

    // Val1 is jailed, so active set should only contain val2
    auto activeSet = manager.getActiveValidatorSet(2);
    requireCondition(activeSet.size() == 1, "Only val2 should be active");
    requireCondition(activeSet[0] == "val2", "Val2 should be active val");

    // Slash val2 by 50%
    manager.slashValidator("val2", 0.5, 100);
    requireCondition(manager.getValidatorStake("val2") == 250, "Val2 stake should be slashed to 250");
    requireCondition(manager.isJailed("val2", 150), "Slashed val2 should be jailed");
}

} // namespace

int main() {
    try {
        testStakingBondAndSelection();
        testStakingUnbonding();
        testJailingAndSlashing();

        std::cout << "Nodo staking manager tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo staking manager tests failed: " << error.what() << "\n";
        return 1;
    }
}
