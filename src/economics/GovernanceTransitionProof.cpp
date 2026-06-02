#include "economics/GovernanceTransitionProof.hpp"

namespace nodo::economics {

std::string GovernanceTransitionProof::build(
    const std::string& governanceProposalId,
    const std::string& transitionId,
    GovernanceLifecycleState fromState,
    GovernanceLifecycleState toState,
    std::uint64_t transitionBlock,
    const std::string& actorId,
    const std::string& policyVersion
) {
    return "governance-transition:"
        + governanceProposalId + ":"
        + transitionId + ":"
        + governanceLifecycleStateToString(fromState) + ":"
        + governanceLifecycleStateToString(toState) + ":"
        + std::to_string(transitionBlock) + ":"
        + actorId + ":"
        + policyVersion;
}

bool GovernanceTransitionProof::verify(
    const std::string& governanceProposalId,
    const std::string& transitionId,
    GovernanceLifecycleState fromState,
    GovernanceLifecycleState toState,
    std::uint64_t transitionBlock,
    const std::string& actorId,
    const std::string& policyVersion,
    const std::string& proof
) {
    return proof == build(
        governanceProposalId,
        transitionId,
        fromState,
        toState,
        transitionBlock,
        actorId,
        policyVersion
    );
}

} // namespace nodo::economics
