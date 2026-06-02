#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_STATE_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_STATE_HPP

#include <string>

namespace nodo::economics {

enum class GovernanceLifecycleState {
    DRAFT,
    SUBMITTED,
    REVIEW,
    VOTING,
    TALLYING,
    DECIDED_APPROVED,
    DECIDED_REJECTED,
    APPROVAL_PRODUCED,
    EXECUTED,
    EXPIRED,
    CANCELLED
};

std::string governanceLifecycleStateToString(GovernanceLifecycleState state);

bool governanceLifecycleStateFromString(
    const std::string& s,
    GovernanceLifecycleState& out
);

// Terminal states have no further valid outgoing transitions.
bool isTerminalGovernanceState(GovernanceLifecycleState state);

// A lifecycle in this state may produce a TreasuryApproval via the bridge.
bool isApprovalEligibleGovernanceState(GovernanceLifecycleState state);

// The lifecycle reached a decided outcome (approved or rejected by tally).
bool isDecidedGovernanceState(GovernanceLifecycleState state);

} // namespace nodo::economics

#endif
