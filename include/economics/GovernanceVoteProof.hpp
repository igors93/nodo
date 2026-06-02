#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_PROOF_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_PROOF_HPP

#include "economics/GovernanceVoteRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

class GovernanceVoteProof {
public:
    static std::string build(
        const std::string& proposalId,
        const std::string& voterId,
        GovernanceVoteChoice choice,
        utils::Amount votingPower,
        std::uint64_t castAtBlock,
        const std::string& policyVersion
    );

    static std::string buildFromRecord(const GovernanceVoteRecord& record);
    static std::string build(const GovernanceVoteRecord& record);
    static bool verify(const GovernanceVoteRecord& record);
};

} // namespace nodo::economics

#endif
