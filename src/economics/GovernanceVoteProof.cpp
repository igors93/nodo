#include "economics/GovernanceVoteProof.hpp"

namespace nodo::economics {

std::string GovernanceVoteProof::build(
    const std::string& proposalId,
    const std::string& voterId,
    GovernanceVoteChoice choice,
    utils::Amount votingPower,
    std::uint64_t castAtBlock,
    const std::string& policyVersion
) {
    return "governance-vote-v1:" +
           proposalId + ":" +
           voterId + ":" +
           governanceVoteChoiceToString(choice) + ":" +
           std::to_string(votingPower.rawUnits()) + ":" +
           std::to_string(castAtBlock) + ":" +
           policyVersion;
}

std::string GovernanceVoteProof::buildFromRecord(
    const GovernanceVoteRecord& record
) {
    return build(
        record.governanceProposalId(),
        record.voterId(),
        record.voteChoice(),
        record.votingPower(),
        record.castAtBlock(),
        record.policyVersion()
    );
}

std::string GovernanceVoteProof::build(const GovernanceVoteRecord& record) {
    return buildFromRecord(record);
}

bool GovernanceVoteProof::verify(const GovernanceVoteRecord& record) {
    return record.voteProof() == buildFromRecord(record);
}

} // namespace nodo::economics
