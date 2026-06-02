#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_PROOF_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_PROOF_HPP

#include "economics/GovernanceVoteRecord.hpp"

#include <string>

namespace nodo::economics {

class GovernanceVoteProof {
public:
    static std::string build(const GovernanceVoteRecord& vote);
};

} // namespace nodo::economics

#endif
