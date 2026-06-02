#include "economics/GovernanceLifecycleStateMachine.hpp"

namespace nodo::economics {

bool GovernanceLifecycleStateMachine::isAllowedTransition(
    GovernanceLifecycleState from,
    GovernanceLifecycleState to
) {
    switch (from) {
        case GovernanceLifecycleState::DRAFT:
            return to == GovernanceLifecycleState::SUBMITTED;

        case GovernanceLifecycleState::SUBMITTED:
            return to == GovernanceLifecycleState::REVIEW ||
                   to == GovernanceLifecycleState::CANCELLED ||
                   to == GovernanceLifecycleState::EXPIRED;

        case GovernanceLifecycleState::REVIEW:
            return to == GovernanceLifecycleState::VOTING ||
                   to == GovernanceLifecycleState::CANCELLED ||
                   to == GovernanceLifecycleState::EXPIRED;

        case GovernanceLifecycleState::VOTING:
            return to == GovernanceLifecycleState::TALLYING ||
                   to == GovernanceLifecycleState::CANCELLED ||
                   to == GovernanceLifecycleState::EXPIRED;

        case GovernanceLifecycleState::TALLYING:
            return to == GovernanceLifecycleState::DECIDED_APPROVED ||
                   to == GovernanceLifecycleState::DECIDED_REJECTED ||
                   to == GovernanceLifecycleState::CANCELLED ||
                   to == GovernanceLifecycleState::EXPIRED;

        case GovernanceLifecycleState::DECIDED_APPROVED:
            return to == GovernanceLifecycleState::APPROVAL_PRODUCED;

        case GovernanceLifecycleState::APPROVAL_PRODUCED:
            return to == GovernanceLifecycleState::EXECUTED;

        // Terminal states: no further transitions allowed.
        case GovernanceLifecycleState::DECIDED_REJECTED:
        case GovernanceLifecycleState::EXECUTED:
        case GovernanceLifecycleState::EXPIRED:
        case GovernanceLifecycleState::CANCELLED:
            return false;

        default:
            return false;
    }
}

} // namespace nodo::economics
