#include "economics/GovernanceLifecycleVerifier.hpp"

#include "economics/GovernanceDecisionAudit.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"

#include <utility>

namespace nodo::economics {

std::string governanceLifecycleVerificationStatusToString(
    GovernanceLifecycleVerificationStatus status
) {
    switch (status) {
        case GovernanceLifecycleVerificationStatus::VERIFIED:
            return "VERIFIED";
        case GovernanceLifecycleVerificationStatus::INVALID_LIFECYCLE:
            return "INVALID_LIFECYCLE";
        case GovernanceLifecycleVerificationStatus::VOTE_AUDIT_FAILED:
            return "VOTE_AUDIT_FAILED";
        case GovernanceLifecycleVerificationStatus::TALLY_MISMATCH:
            return "TALLY_MISMATCH";
        case GovernanceLifecycleVerificationStatus::DECISION_AUDIT_FAILED:
            return "DECISION_AUDIT_FAILED";
        default:
            return "UNKNOWN";
    }
}

GovernanceLifecycleVerificationResult::GovernanceLifecycleVerificationResult()
    : m_verified(false),
      m_status(GovernanceLifecycleVerificationStatus::INVALID_LIFECYCLE) {}

GovernanceLifecycleVerificationResult GovernanceLifecycleVerificationResult::accepted() {
    GovernanceLifecycleVerificationResult result;
    result.m_verified = true;
    result.m_status = GovernanceLifecycleVerificationStatus::VERIFIED;
    return result;
}

GovernanceLifecycleVerificationResult GovernanceLifecycleVerificationResult::rejected(
    GovernanceLifecycleVerificationStatus status,
    std::string reason
) {
    GovernanceLifecycleVerificationResult result;
    result.m_verified = false;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

bool GovernanceLifecycleVerificationResult::verified() const {
    return m_verified;
}
GovernanceLifecycleVerificationStatus GovernanceLifecycleVerificationResult::status() const {
    return m_status;
}
const std::string& GovernanceLifecycleVerificationResult::reason() const {
    return m_reason;
}

namespace {

bool sameTally(
    const GovernanceTallyReport& left,
    const GovernanceTallyReport& right
) {
    return left.governanceProposalId() == right.governanceProposalId() &&
           left.policyVersion() == right.policyVersion() &&
           left.totalVotingPower() == right.totalVotingPower() &&
           left.yesVotingPower() == right.yesVotingPower() &&
           left.noVotingPower() == right.noVotingPower() &&
           left.abstainVotingPower() == right.abstainVotingPower() &&
           left.yesVoteCount() == right.yesVoteCount() &&
           left.noVoteCount() == right.noVoteCount() &&
           left.abstainVoteCount() == right.abstainVoteCount() &&
           left.quorumMet() == right.quorumMet() &&
           left.approvalThresholdMet() == right.approvalThresholdMet() &&
           left.approved() == right.approved() &&
           left.tallyProof() == right.tallyProof();
}

} // namespace

GovernanceLifecycleVerificationResult GovernanceLifecycleVerifier::verify(
    const GovernanceLifecycleRecord& lifecycle
) {
    if (!lifecycle.isValid()) {
        return GovernanceLifecycleVerificationResult::rejected(
            GovernanceLifecycleVerificationStatus::INVALID_LIFECYCLE,
            "GovernanceLifecycleVerifier: lifecycle is invalid: " +
            lifecycle.rejectionReason()
        );
    }

    const GovernanceVoteSetAuditResult voteAudit =
        GovernanceVoteSetAudit::auditVotes(
            lifecycle.votingPolicy(),
            lifecycle.proposalEnvelope().governanceProposalId(),
            lifecycle.voteEvidenceList()
        );

    if (!voteAudit.accepted()) {
        return GovernanceLifecycleVerificationResult::rejected(
            GovernanceLifecycleVerificationStatus::VOTE_AUDIT_FAILED,
            "GovernanceLifecycleVerifier: vote audit failed: " +
            voteAudit.reason()
        );
    }

    if (!sameTally(voteAudit.tallyReport(), lifecycle.tallyReport())) {
        return GovernanceLifecycleVerificationResult::rejected(
            GovernanceLifecycleVerificationStatus::TALLY_MISMATCH,
            "GovernanceLifecycleVerifier: stored tally does not match rebuilt tally."
        );
    }

    const GovernanceDecisionAuditResult decisionAudit =
        GovernanceDecisionAudit::auditDecision(
            lifecycle.proposalEnvelope(),
            lifecycle.votingPolicy(),
            lifecycle.voteEvidenceList(),
            lifecycle.decisionRecord()
        );

    if (!decisionAudit.accepted()) {
        return GovernanceLifecycleVerificationResult::rejected(
            GovernanceLifecycleVerificationStatus::DECISION_AUDIT_FAILED,
            "GovernanceLifecycleVerifier: decision audit failed: " +
            decisionAudit.reason()
        );
    }

    return GovernanceLifecycleVerificationResult::accepted();
}

} // namespace nodo::economics
