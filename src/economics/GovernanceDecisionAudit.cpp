#include "economics/GovernanceDecisionAudit.hpp"

#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"

#include <utility>

namespace nodo::economics {

std::string governanceDecisionAuditStatusToString(
    GovernanceDecisionAuditStatus status
) {
    switch (status) {
        case GovernanceDecisionAuditStatus::ACCEPTED:
            return "ACCEPTED";
        case GovernanceDecisionAuditStatus::VOTE_AUDIT_FAILED:
            return "VOTE_AUDIT_FAILED";
        case GovernanceDecisionAuditStatus::DECISION_REBUILD_FAILED:
            return "DECISION_REBUILD_FAILED";
        case GovernanceDecisionAuditStatus::INVALID_DECISION:
            return "INVALID_DECISION";
        case GovernanceDecisionAuditStatus::DECISION_ID_MISMATCH:
            return "DECISION_ID_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_STATUS_MISMATCH:
            return "DECISION_STATUS_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_PROOF_MISMATCH:
            return "DECISION_PROOF_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_POLICY_VERSION_MISMATCH:
            return "DECISION_POLICY_VERSION_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_BLOCK_MISMATCH:
            return "DECISION_BLOCK_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_MAKER_MISMATCH:
            return "DECISION_MAKER_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_PROPOSAL_MISMATCH:
            return "DECISION_PROPOSAL_MISMATCH";
        case GovernanceDecisionAuditStatus::DECISION_PROPOSAL_TYPE_MISMATCH:
            return "DECISION_PROPOSAL_TYPE_MISMATCH";
        default:
            return "UNKNOWN";
    }
}

GovernanceDecisionAuditResult::GovernanceDecisionAuditResult()
    : m_accepted(false),
      m_status(GovernanceDecisionAuditStatus::VOTE_AUDIT_FAILED) {}

GovernanceDecisionAuditResult GovernanceDecisionAuditResult::ok() {
    GovernanceDecisionAuditResult result;
    result.m_accepted = true;
    result.m_status = GovernanceDecisionAuditStatus::ACCEPTED;
    return result;
}

GovernanceDecisionAuditResult GovernanceDecisionAuditResult::rejected(
    GovernanceDecisionAuditStatus status,
    std::string reason
) {
    GovernanceDecisionAuditResult result;
    result.m_accepted = false;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

bool GovernanceDecisionAuditResult::accepted() const { return m_accepted; }
GovernanceDecisionAuditStatus GovernanceDecisionAuditResult::status() const {
    return m_status;
}
const std::string& GovernanceDecisionAuditResult::reason() const {
    return m_reason;
}

GovernanceDecisionAuditResult GovernanceDecisionAudit::auditDecision(
    const GovernanceProposalEnvelope& envelope,
    const GovernanceVotingPolicy& votingPolicy,
    const std::vector<GovernanceVoteEvidence>& voteEvidenceList,
    const GovernanceDecisionRecord& storedDecision
) {
    if (!envelope.isValid()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_REBUILD_FAILED,
            "GovernanceDecisionAudit: envelope is invalid: " +
            envelope.rejectionReason()
        );
    }

    if (!votingPolicy.isValid()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_REBUILD_FAILED,
            "GovernanceDecisionAudit: voting policy is invalid: " +
            votingPolicy.rejectionReason()
        );
    }

    if (!storedDecision.isValid()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::INVALID_DECISION,
            "GovernanceDecisionAudit: stored decision is invalid: " +
            storedDecision.rejectionReason()
        );
    }

    const GovernanceVoteSetAuditResult voteAudit =
        GovernanceVoteSetAudit::audit(
            envelope.governanceProposalId(),
            votingPolicy,
            voteEvidenceList
        );

    if (!voteAudit.accepted()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::VOTE_AUDIT_FAILED,
            "GovernanceDecisionAudit: vote audit failed: " + voteAudit.reason()
        );
    }

    const GovernanceTallyReport rebuiltTally =
        GovernanceTallyReport::build(voteAudit, votingPolicy);
    if (!rebuiltTally.isValid()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_REBUILD_FAILED,
            "GovernanceDecisionAudit: tally rebuild failed: " +
            rebuiltTally.rejectionReason()
        );
    }

    const GovernanceDecisionBuildResult rebuild =
        GovernanceDecisionBuilder::buildDecision(
            envelope,
            votingPolicy,
            rebuiltTally,
            storedDecision.decidedAtBlock(),
            storedDecision.decisionMaker()
        );

    if (!rebuild.built()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_REBUILD_FAILED,
            "GovernanceDecisionAudit: decision rebuild failed: " + rebuild.reason()
        );
    }

    const GovernanceDecisionRecord& rebuilt = rebuild.decisionRecord();

    if (rebuilt.governanceProposalId() != storedDecision.governanceProposalId()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_PROPOSAL_MISMATCH,
            "GovernanceDecisionAudit: proposal id mismatch."
        );
    }
    if (rebuilt.proposalType() != storedDecision.proposalType()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_PROPOSAL_TYPE_MISMATCH,
            "GovernanceDecisionAudit: proposal type mismatch."
        );
    }
    if (rebuilt.decisionId() != storedDecision.decisionId()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_ID_MISMATCH,
            "GovernanceDecisionAudit: decision id mismatch."
        );
    }
    if (rebuilt.decisionStatus() != storedDecision.decisionStatus()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_STATUS_MISMATCH,
            "GovernanceDecisionAudit: decision status mismatch."
        );
    }
    if (rebuilt.policyVersion() != storedDecision.policyVersion()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_POLICY_VERSION_MISMATCH,
            "GovernanceDecisionAudit: policy version mismatch."
        );
    }
    if (rebuilt.decisionProof() != storedDecision.decisionProof()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_PROOF_MISMATCH,
            "GovernanceDecisionAudit: decision proof mismatch."
        );
    }
    if (rebuilt.decidedAtBlock() != storedDecision.decidedAtBlock()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_BLOCK_MISMATCH,
            "GovernanceDecisionAudit: decidedAtBlock mismatch."
        );
    }
    if (rebuilt.decisionMaker() != storedDecision.decisionMaker()) {
        return GovernanceDecisionAuditResult::rejected(
            GovernanceDecisionAuditStatus::DECISION_MAKER_MISMATCH,
            "GovernanceDecisionAudit: decisionMaker mismatch."
        );
    }

    return GovernanceDecisionAuditResult::ok();
}

} // namespace nodo::economics
