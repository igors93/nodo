#include "economics/GovernanceLifecycleTransitionAudit.hpp"

#include "economics/GovernanceLifecycleStateMachine.hpp"

#include <set>
#include <utility>

namespace nodo::economics {

GovernanceLifecycleTransitionAuditResult::GovernanceLifecycleTransitionAuditResult()
    : m_accepted(false),
      m_finalState(GovernanceLifecycleState::DRAFT) {}

GovernanceLifecycleTransitionAuditResult
GovernanceLifecycleTransitionAuditResult::accepted(GovernanceLifecycleState finalState) {
    GovernanceLifecycleTransitionAuditResult r;
    r.m_accepted = true;
    r.m_finalState = finalState;
    return r;
}

GovernanceLifecycleTransitionAuditResult
GovernanceLifecycleTransitionAuditResult::rejected(std::string reason) {
    GovernanceLifecycleTransitionAuditResult r;
    r.m_accepted = false;
    r.m_reason = std::move(reason);
    r.m_finalState = GovernanceLifecycleState::DRAFT;
    return r;
}

bool GovernanceLifecycleTransitionAuditResult::accepted() const { return m_accepted; }
const std::string& GovernanceLifecycleTransitionAuditResult::reason() const { return m_reason; }
GovernanceLifecycleState GovernanceLifecycleTransitionAuditResult::finalState() const {
    return m_finalState;
}

GovernanceLifecycleTransitionAuditResult GovernanceLifecycleTransitionAudit::audit(
    const std::vector<GovernanceLifecycleTransition>& transitions,
    const std::string& expectedProposalId,
    const std::string& expectedPolicyVersion
) {
    if (transitions.empty()) {
        return GovernanceLifecycleTransitionAuditResult::rejected(
            "GovernanceLifecycleTransitionAudit: transition history must not be empty."
        );
    }

    std::set<std::string> seenIds;
    GovernanceLifecycleState currentState = GovernanceLifecycleState::DRAFT;
    std::uint64_t prevBlock = 0;
    std::string prevIdAtSameBlock;

    for (std::size_t i = 0; i < transitions.size(); ++i) {
        const GovernanceLifecycleTransition& t = transitions[i];

        // Structural validity (proof included).
        if (!t.isValid()) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] is invalid: " + t.rejectionReason()
            );
        }

        // Proposal binding.
        if (t.governanceProposalId() != expectedProposalId) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] proposalId='" + t.governanceProposalId() +
                "' does not match expected='" + expectedProposalId + "'."
            );
        }

        // Policy version consistency.
        if (t.policyVersion() != expectedPolicyVersion) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] policyVersion='" + t.policyVersion() +
                "' does not match expected='" + expectedPolicyVersion + "'."
            );
        }

        // No duplicate transitionId.
        if (!seenIds.insert(t.transitionId()).second) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: duplicate transitionId='" +
                t.transitionId() + "' at index " + std::to_string(i) + "."
            );
        }

        // Non-decreasing block order; within same block, transitionId must be lexicographically ordered.
        if (t.transitionBlock() < prevBlock) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] transitionBlock=" +
                std::to_string(t.transitionBlock()) +
                " is less than previous block=" + std::to_string(prevBlock) + "."
            );
        }
        if (t.transitionBlock() == prevBlock && i > 0 &&
            t.transitionId() <= prevIdAtSameBlock) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] at same block must have strictly greater transitionId."
            );
        }

        // fromState must match current rebuilt state.
        if (t.fromState() != currentState) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] fromState='" +
                governanceLifecycleStateToString(t.fromState()) +
                "' does not match current rebuilt state='" +
                governanceLifecycleStateToString(currentState) + "'."
            );
        }

        // State machine allows this transition.
        if (!GovernanceLifecycleStateMachine::isAllowedTransition(
                t.fromState(), t.toState())) {
            return GovernanceLifecycleTransitionAuditResult::rejected(
                "GovernanceLifecycleTransitionAudit: transition[" +
                std::to_string(i) + "] from='" +
                governanceLifecycleStateToString(t.fromState()) + "' to='" +
                governanceLifecycleStateToString(t.toState()) +
                "' is not an allowed transition."
            );
        }

        prevIdAtSameBlock = (t.transitionBlock() == prevBlock) ? t.transitionId() : "";
        prevBlock = t.transitionBlock();
        currentState = t.toState();
    }

    return GovernanceLifecycleTransitionAuditResult::accepted(currentState);
}

} // namespace nodo::economics
