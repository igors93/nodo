#include "economics/GovernanceLifecycleState.hpp"

namespace nodo::economics {

std::string governanceLifecycleStateToString(GovernanceLifecycleState state) {
    switch (state) {
        case GovernanceLifecycleState::DRAFT:             return "DRAFT";
        case GovernanceLifecycleState::SUBMITTED:         return "SUBMITTED";
        case GovernanceLifecycleState::REVIEW:            return "REVIEW";
        case GovernanceLifecycleState::VOTING:            return "VOTING";
        case GovernanceLifecycleState::TALLYING:          return "TALLYING";
        case GovernanceLifecycleState::DECIDED_APPROVED:  return "DECIDED_APPROVED";
        case GovernanceLifecycleState::DECIDED_REJECTED:  return "DECIDED_REJECTED";
        case GovernanceLifecycleState::APPROVAL_PRODUCED: return "APPROVAL_PRODUCED";
        case GovernanceLifecycleState::EXECUTED:          return "EXECUTED";
        case GovernanceLifecycleState::EXPIRED:           return "EXPIRED";
        case GovernanceLifecycleState::CANCELLED:         return "CANCELLED";
        default:                                           return "UNKNOWN";
    }
}

bool governanceLifecycleStateFromString(
    const std::string& s,
    GovernanceLifecycleState& out
) {
    if (s == "DRAFT")             { out = GovernanceLifecycleState::DRAFT;             return true; }
    if (s == "SUBMITTED")         { out = GovernanceLifecycleState::SUBMITTED;         return true; }
    if (s == "REVIEW")            { out = GovernanceLifecycleState::REVIEW;            return true; }
    if (s == "VOTING")            { out = GovernanceLifecycleState::VOTING;            return true; }
    if (s == "TALLYING")          { out = GovernanceLifecycleState::TALLYING;          return true; }
    if (s == "DECIDED_APPROVED")  { out = GovernanceLifecycleState::DECIDED_APPROVED;  return true; }
    if (s == "DECIDED_REJECTED")  { out = GovernanceLifecycleState::DECIDED_REJECTED;  return true; }
    if (s == "APPROVAL_PRODUCED") { out = GovernanceLifecycleState::APPROVAL_PRODUCED; return true; }
    if (s == "EXECUTED")          { out = GovernanceLifecycleState::EXECUTED;          return true; }
    if (s == "EXPIRED")           { out = GovernanceLifecycleState::EXPIRED;           return true; }
    if (s == "CANCELLED")         { out = GovernanceLifecycleState::CANCELLED;         return true; }
    return false;
}

bool isTerminalGovernanceState(GovernanceLifecycleState state) {
    switch (state) {
        case GovernanceLifecycleState::DECIDED_REJECTED:
        case GovernanceLifecycleState::EXECUTED:
        case GovernanceLifecycleState::EXPIRED:
        case GovernanceLifecycleState::CANCELLED:
            return true;
        default:
            return false;
    }
}

bool isApprovalEligibleGovernanceState(GovernanceLifecycleState state) {
    return state == GovernanceLifecycleState::DECIDED_APPROVED;
}

bool isDecidedGovernanceState(GovernanceLifecycleState state) {
    return state == GovernanceLifecycleState::DECIDED_APPROVED ||
           state == GovernanceLifecycleState::DECIDED_REJECTED;
}

} // namespace nodo::economics
