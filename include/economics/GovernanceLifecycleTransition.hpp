#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_TRANSITION_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_TRANSITION_HPP

#include "economics/GovernanceLifecycleState.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GovernanceLifecycleTransition records a single verified state transition in
 * a governance lifecycle. The transitionProof binds all fields deterministically
 * so that any node can verify the transition without trusting stored state.
 */
class GovernanceLifecycleTransition {
public:
    GovernanceLifecycleTransition();

    GovernanceLifecycleTransition(
        std::string transitionId,
        std::string governanceProposalId,
        GovernanceLifecycleState fromState,
        GovernanceLifecycleState toState,
        std::uint64_t transitionBlock,
        std::string actorId,
        std::string reason,
        std::string transitionProof,
        std::string policyVersion
    );

    const std::string& transitionId() const;
    const std::string& governanceProposalId() const;
    GovernanceLifecycleState fromState() const;
    GovernanceLifecycleState toState() const;
    std::uint64_t transitionBlock() const;
    const std::string& actorId() const;
    const std::string& reason() const;
    const std::string& transitionProof() const;
    const std::string& policyVersion() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

private:
    std::string m_transitionId;
    std::string m_governanceProposalId;
    GovernanceLifecycleState m_fromState;
    GovernanceLifecycleState m_toState;
    std::uint64_t m_transitionBlock;
    std::string m_actorId;
    std::string m_reason;
    std::string m_transitionProof;
    std::string m_policyVersion;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
