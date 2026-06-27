// P1 tests: Defense Mode integration with TreasurySpendValidator and DefenseModeGuard.
#include "economics/DefenseModeState.hpp"
#include "economics/TreasuryAccount.hpp"
#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryPolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "economics/TreasurySpendValidator.hpp"
#include "node/DefenseModeGuard.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::DefenseModePolicy;
using nodo::economics::DefenseModeState;
using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendStatus;
using nodo::economics::TreasurySpendValidator;
using nodo::node::DefenseModeGuard;
using nodo::node::DefenseGateStatus;
using nodo::utils::Amount;

TreasuryAccount validTreasury() {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr",
        Amount::fromRawUnits(1000000), 0, false, ""
    );
}

TreasuryPolicy validPolicy() {
    return TreasuryPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000),
        Amount::fromRawUnits(100000),
        5, true, false
    );
}

TreasuryProposal validProposal() {
    return TreasuryProposal(
        "prop-001", "recipient-addr", Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

TreasuryApproval validApproval() {
    return TreasuryApproval(
        "appr-001", "prop-001", 3,
        "governance-node", "valid-proof-abc"
    );
}

// Test 1: Defense Mode ACTIVE blocks treasury spend.
void testDefenseModeActiveBlocksTreasurySpend() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = TreasurySpendValidator::validateSpend(
        DefenseModeState::ACTIVE,
        policy,
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::DEFENSE_MODE_ACTIVE);
}

// Test 2: Defense Mode INACTIVE allows normal treasury spend.
void testDefenseModeInactiveAllowsNormalSpend() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = TreasurySpendValidator::validateSpend(
        DefenseModeState::INACTIVE,
        policy,
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    assert(result.status() == TreasurySpendStatus::ACCEPTED);
}

// Test 3: Defense Mode ACTIVE but policy says don't block treasury — allows spend.
void testDefenseModeActiveButPolicyAllowsSpend() {
    const DefenseModePolicy permissivePolicy(
        false, // blockTreasurySpend = false
        true,  // blockExtraordinaryMint
        true,  // blockExtraordinaryRewards
        true,  // requireChainAuditBeforeExit
        true   // allowNormalTransactions
    );
    const auto result = TreasurySpendValidator::validateSpend(
        DefenseModeState::ACTIVE,
        permissivePolicy,
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(result.accepted());
}

// Test 4: DefenseModeGuard blocks extraordinary mint when ACTIVE.
void testGuardBlocksExtraordinaryMint() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkExtraordinaryMint(
        DefenseModeState::ACTIVE, policy
    );
    assert(!result.isAllowed());
    assert(result.status() == DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE);
}

// Test 5: DefenseModeGuard allows extraordinary mint when INACTIVE.
void testGuardAllowsExtraordinaryMintWhenInactive() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkExtraordinaryMint(
        DefenseModeState::INACTIVE, policy
    );
    assert(result.isAllowed());
}

// Test 6: DefenseModeGuard blocks extraordinary reward when ACTIVE.
void testGuardBlocksExtraordinaryReward() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkExtraordinaryReward(
        DefenseModeState::ACTIVE, policy
    );
    assert(!result.isAllowed());
    assert(result.status() == DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE);
}

// Test 7: DefenseModeGuard allows extraordinary reward when INACTIVE.
void testGuardAllowsExtraordinaryRewardWhenInactive() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkExtraordinaryReward(
        DefenseModeState::INACTIVE, policy
    );
    assert(result.isAllowed());
}

} // namespace

int main() {
    testDefenseModeActiveBlocksTreasurySpend();
    testDefenseModeInactiveAllowsNormalSpend();
    testDefenseModeActiveButPolicyAllowsSpend();
    testGuardBlocksExtraordinaryMint();
    testGuardAllowsExtraordinaryMintWhenInactive();
    testGuardBlocksExtraordinaryReward();
    testGuardAllowsExtraordinaryRewardWhenInactive();
    return 0;
}
