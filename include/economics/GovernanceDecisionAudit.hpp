#ifndef NODO_ECONOMICS_GOVERNANCE_DECISION_AUDIT_HPP
#define NODO_ECONOMICS_GOVERNANCE_DECISION_AUDIT_HPP

#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernanceVoteEvidence.hpp"
#include "economics/GovernanceVotingPolicy.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::economics {

enum class GovernanceDecisionAuditStatus {
    ACCEPTED,
    VOTE_AUDIT_FAILED,
    DECISION_REBUILD_FAILED,
    INVALID_DECISION,
    DECISION_ID_MISMATCH,
    DECISION_STATUS_MISMATCH,
    DECISION_PROOF_MISMATCH,
    DECISION_POLICY_VERSION_MISMATCH,
    DECISION_BLOCK_MISMATCH,
    DECISION_MAKER_MISMATCH,
    DECISION_PROPOSAL_MISMATCH,
    DECISION_PROPOSAL_TYPE_MISMATCH
};

std::string governanceDecisionAuditStatusToString(
    GovernanceDecisionAuditStatus status
);

class GovernanceDecisionAuditResult {
public:
    GovernanceDecisionAuditResult();

    static GovernanceDecisionAuditResult ok();

    static GovernanceDecisionAuditResult rejected(
        GovernanceDecisionAuditStatus status,
        std::string reason
    );

    bool accepted() const;
    GovernanceDecisionAuditStatus status() const;
    const std::string& reason() const;

private:
    bool m_accepted;
    GovernanceDecisionAuditStatus m_status;
    std::string m_reason;
};

class GovernanceDecisionAudit {
public:
    static GovernanceDecisionAuditResult auditDecision(
        const GovernanceProposalEnvelope& envelope,
        const GovernanceVotingPolicy& votingPolicy,
        const std::vector<GovernanceVoteEvidence>& voteEvidenceList,
        const GovernanceDecisionRecord& storedDecision
    );
};

} // namespace nodo::economics

#endif
