#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_SET_AUDIT_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_SET_AUDIT_HPP

#include "economics/GovernanceVoteEvidence.hpp"
#include "economics/GovernanceVotingPolicy.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::economics {

enum class GovernanceVoteSetAuditStatus {
    ACCEPTED,
    INVALID_VOTING_POLICY,
    EMPTY_VOTE_SET,
    INVALID_VOTE_EVIDENCE,
    PROPOSAL_MISMATCH,
    POLICY_VERSION_MISMATCH,
    DUPLICATE_EVIDENCE_ID,
    DUPLICATE_VOTE_ID,
    DUPLICATE_VOTER,
    REPLACEMENT_NOT_IMPLEMENTED
};

std::string governanceVoteSetAuditStatusToString(
    GovernanceVoteSetAuditStatus status
);

class GovernanceVoteSetAuditResult {
public:
    GovernanceVoteSetAuditResult();

    static GovernanceVoteSetAuditResult accepted(
        std::vector<GovernanceVoteEvidence> canonicalVotes
    );

    static GovernanceVoteSetAuditResult rejected(
        GovernanceVoteSetAuditStatus status,
        std::string reason,
        std::size_t failedAtIndex = 0
    );

    bool accepted() const;
    bool isValid() const;
    GovernanceVoteSetAuditStatus status() const;
    const std::string& reason() const;
    std::size_t failedAtIndex() const;
    const std::vector<GovernanceVoteEvidence>& canonicalVotes() const;

private:
    bool m_accepted;
    GovernanceVoteSetAuditStatus m_status;
    std::string m_reason;
    std::size_t m_failedAtIndex;
    std::vector<GovernanceVoteEvidence> m_canonicalVotes;
};

class GovernanceVoteSetAudit {
public:
    static GovernanceVoteSetAuditResult audit(
        const std::string& governanceProposalId,
        const GovernanceVotingPolicy& votingPolicy,
        const std::vector<GovernanceVoteEvidence>& voteEvidenceList
    );

    static GovernanceVoteSetAuditResult auditVotes(
        const GovernanceVotingPolicy& votingPolicy,
        const std::string& governanceProposalId,
        const std::vector<GovernanceVoteEvidence>& voteEvidenceList
    );
};

} // namespace nodo::economics

#endif
