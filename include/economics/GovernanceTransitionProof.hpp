#ifndef NODO_ECONOMICS_GOVERNANCE_TRANSITION_PROOF_HPP
#define NODO_ECONOMICS_GOVERNANCE_TRANSITION_PROOF_HPP

#include "economics/GovernanceLifecycleState.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GovernanceTransitionProof produces and verifies deterministic proofs for
 * governance lifecycle state transitions.
 *
 * Security principle:
 * Proof format: governance-transition:<proposalId>:<transitionId>:<fromState>:<toState>:<block>:<actorId>:<policyVersion>
 * The same inputs always produce the same proof. Changing any input field
 * changes the proof. No randomness is used.
 */
class GovernanceTransitionProof {
public:
    static std::string build(
        const std::string& governanceProposalId,
        const std::string& transitionId,
        GovernanceLifecycleState fromState,
        GovernanceLifecycleState toState,
        std::uint64_t transitionBlock,
        const std::string& actorId,
        const std::string& policyVersion
    );

    static bool verify(
        const std::string& governanceProposalId,
        const std::string& transitionId,
        GovernanceLifecycleState fromState,
        GovernanceLifecycleState toState,
        std::uint64_t transitionBlock,
        const std::string& actorId,
        const std::string& policyVersion,
        const std::string& proof
    );
};

} // namespace nodo::economics

#endif
