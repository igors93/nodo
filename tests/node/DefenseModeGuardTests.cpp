#include "node/DefenseModeGuard.hpp"

#include "economics/DefenseModeState.hpp"

#include <cassert>

namespace {

using nodo::economics::DefenseModePolicy;
using nodo::economics::DefenseModeState;
using nodo::node::DefenseModeGuard;
using nodo::node::DefenseGateStatus;

void testInactiveAllowsAllOperations() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();

    const auto ts = DefenseModeGuard::checkTreasurySpend(DefenseModeState::INACTIVE, policy);
    assert(ts.isAllowed());
    assert(ts.status() == DefenseGateStatus::ALLOWED);

    const auto em = DefenseModeGuard::checkExtraordinaryMint(DefenseModeState::INACTIVE, policy);
    assert(em.isAllowed());

    const auto ga = DefenseModeGuard::checkGovernanceApproval(DefenseModeState::INACTIVE);
    assert(ga.isAllowed());
}

void testActiveBlocksTreasurySpend() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkTreasurySpend(DefenseModeState::ACTIVE, policy);
    assert(!result.isAllowed());
    assert(result.status() == DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE);
    assert(!result.reason().empty());
}

void testActiveBlocksExtraordinaryMint() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    const auto result = DefenseModeGuard::checkExtraordinaryMint(DefenseModeState::ACTIVE, policy);
    assert(!result.isAllowed());
    assert(result.status() == DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE);
}

void testActiveBlocksGovernanceApproval() {
    const auto result = DefenseModeGuard::checkGovernanceApproval(DefenseModeState::ACTIVE);
    assert(!result.isAllowed());
    assert(result.status() == DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE);
}

void testCustomPolicyCanAllowTreasurySpend() {
    // A policy that does NOT block treasury spend.
    const DefenseModePolicy policy(false, true, true, true, true);
    const auto result = DefenseModeGuard::checkTreasurySpend(DefenseModeState::ACTIVE, policy);
    assert(result.isAllowed());
}

} // namespace

int main() {
    testInactiveAllowsAllOperations();
    testActiveBlocksTreasurySpend();
    testActiveBlocksExtraordinaryMint();
    testActiveBlocksGovernanceApproval();
    testCustomPolicyCanAllowTreasurySpend();
    return 0;
}
