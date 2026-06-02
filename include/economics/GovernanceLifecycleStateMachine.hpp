#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_STATE_MACHINE_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_STATE_MACHINE_HPP

#include "economics/GovernanceLifecycleState.hpp"

namespace nodo::economics {

/*
 * GovernanceLifecycleStateMachine enforces that no state transition can skip
 * required lifecycle stages. Attackers cannot forge a direct jump from
 * SUBMITTED to DECIDED_APPROVED without passing through REVIEW, VOTING, and
 * TALLYING.
 */
class GovernanceLifecycleStateMachine {
public:
    static bool isAllowedTransition(
        GovernanceLifecycleState from,
        GovernanceLifecycleState to
    );
};

} // namespace nodo::economics

#endif
