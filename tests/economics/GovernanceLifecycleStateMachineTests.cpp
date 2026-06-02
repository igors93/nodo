#include "economics/GovernanceLifecycleStateMachine.hpp"

#include "economics/GovernanceLifecycleState.hpp"

#include <cassert>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceLifecycleStateMachine;

void testAllowedTransitions() {
    using S = GovernanceLifecycleState;
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::DRAFT,             S::SUBMITTED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,         S::REVIEW));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::REVIEW,            S::VOTING));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::VOTING,            S::TALLYING));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::TALLYING,          S::DECIDED_APPROVED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::TALLYING,          S::DECIDED_REJECTED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::DECIDED_APPROVED,  S::APPROVAL_PRODUCED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::APPROVAL_PRODUCED, S::EXECUTED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,         S::CANCELLED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::REVIEW,            S::CANCELLED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::VOTING,            S::CANCELLED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::TALLYING,          S::CANCELLED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,         S::EXPIRED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::REVIEW,            S::EXPIRED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::VOTING,            S::EXPIRED));
    assert(GovernanceLifecycleStateMachine::isAllowedTransition(S::TALLYING,          S::EXPIRED));
}

void testDirectJumpsRejected() {
    using S = GovernanceLifecycleState;
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::DRAFT,            S::VOTING));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::DRAFT,            S::DECIDED_APPROVED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,        S::DECIDED_APPROVED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::REVIEW,           S::DECIDED_APPROVED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::VOTING,           S::DECIDED_APPROVED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,        S::TALLYING));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::DRAFT,            S::APPROVAL_PRODUCED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::SUBMITTED,        S::EXECUTED));
}

void testRejectedDecisionCannotProduceApproval() {
    using S = GovernanceLifecycleState;
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::DECIDED_REJECTED, S::APPROVAL_PRODUCED));
}

void testCancelledCannotProduceApproval() {
    using S = GovernanceLifecycleState;
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::CANCELLED,        S::APPROVAL_PRODUCED));
}

void testExpiredCannotProduceApproval() {
    using S = GovernanceLifecycleState;
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::EXPIRED,          S::APPROVAL_PRODUCED));
}

void testTerminalStatesCannotTransitionFurther() {
    using S = GovernanceLifecycleState;
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::EXECUTED,         S::DRAFT));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::EXECUTED,         S::SUBMITTED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::EXPIRED,          S::SUBMITTED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::CANCELLED,        S::SUBMITTED));
    assert(!GovernanceLifecycleStateMachine::isAllowedTransition(S::DECIDED_REJECTED, S::SUBMITTED));
}

} // namespace

int main() {
    testAllowedTransitions();
    testDirectJumpsRejected();
    testRejectedDecisionCannotProduceApproval();
    testCancelledCannotProduceApproval();
    testExpiredCannotProduceApproval();
    testTerminalStatesCannotTransitionFurther();
    return 0;
}
