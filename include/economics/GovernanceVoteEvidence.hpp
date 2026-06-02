#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_EVIDENCE_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_EVIDENCE_HPP

#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/GovernanceVoteRecord.hpp"
#include "economics/GovernanceVotingPolicy.hpp"

#include <string>

namespace nodo::economics {

class GovernanceVoteEvidence {
public:
    GovernanceVoteEvidence();

    GovernanceVoteEvidence(
        std::string evidenceId,
        GovernanceProposalEnvelope proposalEnvelope,
        GovernanceVotingPolicy votingPolicy,
        GovernanceVoteRecord voteRecord
    );

    const std::string& evidenceId() const;
    const GovernanceProposalEnvelope& proposalEnvelope() const;
    const GovernanceVotingPolicy& votingPolicy() const;
    const GovernanceVoteRecord& voteRecord() const;

    bool isValid() const;
    std::string rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_evidenceId;
    GovernanceProposalEnvelope m_proposalEnvelope;
    GovernanceVotingPolicy m_votingPolicy;
    GovernanceVoteRecord m_voteRecord;
};

} // namespace nodo::economics

#endif
