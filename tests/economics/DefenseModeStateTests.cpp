#include "economics/DefenseModeState.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::DefenseModePolicy;
using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::economics::defenseModeStateToString;
using nodo::economics::defenseModeStateFromString;
using nodo::economics::defenseModeTriggerToString;

void testStateRoundTrip() {
    DefenseModeState out = DefenseModeState::ACTIVE;
    assert(defenseModeStateFromString("INACTIVE", out));
    assert(out == DefenseModeState::INACTIVE);

    assert(defenseModeStateFromString("ACTIVE", out));
    assert(out == DefenseModeState::ACTIVE);
}

void testUnknownStateRejected() {
    DefenseModeState out = DefenseModeState::INACTIVE;
    assert(!defenseModeStateFromString("UNKNOWN", out));
    assert(!defenseModeStateFromString("", out));
}

void testStateToString() {
    assert(defenseModeStateToString(DefenseModeState::INACTIVE) == "INACTIVE");
    assert(defenseModeStateToString(DefenseModeState::ACTIVE)   == "ACTIVE");
}

void testTriggerToString() {
    assert(defenseModeTriggerToString(DefenseModeTrigger::SUPPLY_DIVERGENCE) == "SUPPLY_DIVERGENCE");
    assert(defenseModeTriggerToString(DefenseModeTrigger::CHAIN_AUDIT_FAILURE) == "CHAIN_AUDIT_FAILURE");
    assert(defenseModeTriggerToString(DefenseModeTrigger::GOVERNANCE_VOTED) == "GOVERNANCE_VOTED");
}

void testDefaultPolicyBlocksEconomicActions() {
    const DefenseModePolicy policy = DefenseModePolicy::defaultPolicy();
    assert(policy.blockTreasurySpend());
    assert(policy.blockExtraordinaryMint());
    assert(policy.requireChainAuditBeforeExit());
    assert(policy.allowNormalTransactions());
}

void testCustomPolicy() {
    const DefenseModePolicy policy(false, true, true, false);
    assert(!policy.blockTreasurySpend());
    assert(policy.blockExtraordinaryMint());
    assert(policy.requireChainAuditBeforeExit());
    assert(!policy.allowNormalTransactions());
}

} // namespace

int main() {
    testStateRoundTrip();
    testUnknownStateRejected();
    testStateToString();
    testTriggerToString();
    testDefaultPolicyBlocksEconomicActions();
    testCustomPolicy();
    return 0;
}
