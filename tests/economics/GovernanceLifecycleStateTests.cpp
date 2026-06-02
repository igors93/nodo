#include "economics/GovernanceLifecycleState.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::governanceLifecycleStateToString;
using nodo::economics::governanceLifecycleStateFromString;
using nodo::economics::isTerminalGovernanceState;
using nodo::economics::isApprovalEligibleGovernanceState;
using nodo::economics::isDecidedGovernanceState;

void testRoundTrip() {
    const GovernanceLifecycleState states[] = {
        GovernanceLifecycleState::DRAFT,
        GovernanceLifecycleState::SUBMITTED,
        GovernanceLifecycleState::REVIEW,
        GovernanceLifecycleState::VOTING,
        GovernanceLifecycleState::TALLYING,
        GovernanceLifecycleState::DECIDED_APPROVED,
        GovernanceLifecycleState::DECIDED_REJECTED,
        GovernanceLifecycleState::APPROVAL_PRODUCED,
        GovernanceLifecycleState::EXECUTED,
        GovernanceLifecycleState::EXPIRED,
        GovernanceLifecycleState::CANCELLED
    };
    for (const auto& s : states) {
        GovernanceLifecycleState parsed = GovernanceLifecycleState::DRAFT;
        assert(governanceLifecycleStateFromString(governanceLifecycleStateToString(s), parsed));
        assert(parsed == s);
    }
}

void testUnknownStateRejected() {
    GovernanceLifecycleState out = GovernanceLifecycleState::DRAFT;
    assert(!governanceLifecycleStateFromString("UNKNOWN_STATE", out));
    assert(!governanceLifecycleStateFromString("", out));
    assert(!governanceLifecycleStateFromString("draft", out));  // case-sensitive
}

void testTerminalStateClassification() {
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::DRAFT));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::SUBMITTED));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::REVIEW));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::VOTING));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::TALLYING));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::DECIDED_APPROVED));
    assert(isTerminalGovernanceState(GovernanceLifecycleState::DECIDED_REJECTED));
    assert(!isTerminalGovernanceState(GovernanceLifecycleState::APPROVAL_PRODUCED));
    assert(isTerminalGovernanceState(GovernanceLifecycleState::EXECUTED));
    assert(isTerminalGovernanceState(GovernanceLifecycleState::EXPIRED));
    assert(isTerminalGovernanceState(GovernanceLifecycleState::CANCELLED));
}

void testApprovalEligibleClassification() {
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::DRAFT));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::SUBMITTED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::REVIEW));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::VOTING));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::TALLYING));
    assert(isApprovalEligibleGovernanceState(GovernanceLifecycleState::DECIDED_APPROVED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::DECIDED_REJECTED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::APPROVAL_PRODUCED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::EXECUTED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::EXPIRED));
    assert(!isApprovalEligibleGovernanceState(GovernanceLifecycleState::CANCELLED));
}

void testDecidedStateClassification() {
    assert(!isDecidedGovernanceState(GovernanceLifecycleState::DRAFT));
    assert(!isDecidedGovernanceState(GovernanceLifecycleState::SUBMITTED));
    assert(!isDecidedGovernanceState(GovernanceLifecycleState::TALLYING));
    assert(isDecidedGovernanceState(GovernanceLifecycleState::DECIDED_APPROVED));
    assert(isDecidedGovernanceState(GovernanceLifecycleState::DECIDED_REJECTED));
    assert(!isDecidedGovernanceState(GovernanceLifecycleState::APPROVAL_PRODUCED));
    assert(!isDecidedGovernanceState(GovernanceLifecycleState::EXECUTED));
}

} // namespace

int main() {
    testRoundTrip();
    testUnknownStateRejected();
    testTerminalStateClassification();
    testApprovalEligibleClassification();
    testDecidedStateClassification();
    return 0;
}
