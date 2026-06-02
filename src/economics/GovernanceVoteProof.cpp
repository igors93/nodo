#include "economics/GovernanceVoteProof.hpp"

namespace nodo::economics {

std::string GovernanceVoteProof::build(const GovernanceVoteRecord& vote) {
    return "governance-vote:"
        + vote.governanceProposalId() + ":"
        + vote.voteId() + ":"
        + vote.voterId() + ":"
        + governanceVoteChoiceToString(vote.choice()) + ":"
        + std::to_string(vote.votingPower()) + ":"
        + std::to_string(vote.votedAtBlock()) + ":"
        + vote.policyVersion();
}

} // namespace nodo::economics
