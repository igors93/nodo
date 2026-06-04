#include "economics/GovernanceTransitionProof.hpp"

#include "crypto/hash.h"

namespace nodo::economics {

namespace {

std::string hashCanonical(const std::string& canonical) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(canonical.c_str(), output, sizeof(output));
    return std::string(output);
}

std::string canonicalString(
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

} // namespace

std::string GovernanceTransitionProof::build(
    const std::string& governanceProposalId,
    const std::string& transitionId,
    GovernanceLifecycleState fromState,
    GovernanceLifecycleState toState,
    std::uint64_t transitionBlock,
    const std::string& actorId,
    const std::string& policyVersion
) {
    return hashCanonical(canonicalString(
        governanceProposalId,
        transitionId,
        fromState,
        toState,
        transitionBlock,
        actorId,
        policyVersion
    ));
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
