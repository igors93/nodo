#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_EVIDENCE_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_EVIDENCE_HPP

#include "economics/GovernanceVoteRecord.hpp"

#include <string>

namespace nodo::economics {

class GovernanceVoteEvidence {
public:
    GovernanceVoteEvidence();

    GovernanceVoteEvidence(
        GovernanceVoteRecord voteRecord,
        std::string voteProof
    );

    const GovernanceVoteRecord& voteRecord() const;
    const std::string& voteProof() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    GovernanceVoteRecord m_voteRecord;
    std::string m_voteProof;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
