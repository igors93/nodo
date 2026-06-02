#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_TRANSITION_AUDIT_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_TRANSITION_AUDIT_HPP

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleTransition.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

class GovernanceLifecycleTransitionAuditResult {
public:
    GovernanceLifecycleTransitionAuditResult();

    static GovernanceLifecycleTransitionAuditResult accepted(
        GovernanceLifecycleState finalState
    );

    static GovernanceLifecycleTransitionAuditResult rejected(std::string reason);

    bool accepted() const;
    const std::string& reason() const;
    GovernanceLifecycleState finalState() const;

private:
    bool m_accepted;
    std::string m_reason;
    GovernanceLifecycleState m_finalState;
};

/*
 * GovernanceLifecycleTransitionAudit verifies a complete sequence of lifecycle
 * transitions from their initial to final state.
 *
 * Security principle:
 * Each transition must be structurally valid, carry a verified proof, belong to
 * the expected proposal, carry the expected policy version, have no duplicate
 * transitionId, be in non-decreasing block order, start from where the previous
 * transition ended, and be allowed by GovernanceLifecycleStateMachine. Any
 * violation rejects the entire sequence, preventing state injection, replay,
 * and forged path attacks.
 */
class GovernanceLifecycleTransitionAudit {
public:
    static GovernanceLifecycleTransitionAuditResult audit(
        const std::vector<GovernanceLifecycleTransition>& transitions,
        const std::string& expectedProposalId,
        const std::string& expectedPolicyVersion
    );
};

} // namespace nodo::economics

#endif
