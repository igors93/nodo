#include "economics/GovernanceApprovalBridge.hpp"

#include "economics/TreasuryApprovalProof.hpp"

#include <utility>

namespace nodo::economics {

std::string governanceApprovalBridgeStatusToString(GovernanceApprovalBridgeStatus status) {
    switch (status) {
        case GovernanceApprovalBridgeStatus::ACCEPTED:
            return "ACCEPTED";
        case GovernanceApprovalBridgeStatus::INVALID_POLICY:
            return "INVALID_POLICY";
        case GovernanceApprovalBridgeStatus::INVALID_ENVELOPE:
            return "INVALID_ENVELOPE";
        case GovernanceApprovalBridgeStatus::INVALID_DECISION:
            return "INVALID_DECISION";
        case GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH:
            return "POLICY_VERSION_MISMATCH";
        case GovernanceApprovalBridgeStatus::PROPOSAL_MISMATCH:
            return "PROPOSAL_MISMATCH";
        case GovernanceApprovalBridgeStatus::DECISION_NOT_APPROVED:
            return "DECISION_NOT_APPROVED";
        case GovernanceApprovalBridgeStatus::REVIEW_PERIOD_NOT_SATISFIED:
            return "REVIEW_PERIOD_NOT_SATISFIED";
        case GovernanceApprovalBridgeStatus::DECISION_PROOF_REQUIRED:
            return "DECISION_PROOF_REQUIRED";
        default:
            return "UNKNOWN";
    }
}

GovernanceApprovalBridgeResult::GovernanceApprovalBridgeResult()
    : m_status(GovernanceApprovalBridgeStatus::INVALID_POLICY),
      m_reason("") {}

GovernanceApprovalBridgeResult GovernanceApprovalBridgeResult::accepted(
    TreasuryApproval approval
) {
    GovernanceApprovalBridgeResult r;
    r.m_status = GovernanceApprovalBridgeStatus::ACCEPTED;
    r.m_treasuryApproval = std::move(approval);
    return r;
}

GovernanceApprovalBridgeResult GovernanceApprovalBridgeResult::rejected(
    GovernanceApprovalBridgeStatus status,
    std::string reason
) {
    GovernanceApprovalBridgeResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool GovernanceApprovalBridgeResult::isAccepted() const {
    return m_status == GovernanceApprovalBridgeStatus::ACCEPTED;
}
GovernanceApprovalBridgeStatus GovernanceApprovalBridgeResult::status() const {
    return m_status;
}
const std::string& GovernanceApprovalBridgeResult::reason() const { return m_reason; }
const TreasuryApproval& GovernanceApprovalBridgeResult::treasuryApproval() const {
    return m_treasuryApproval;
}

GovernanceApprovalBridgeResult GovernanceApprovalBridge::produceTreasuryApproval(
    const GovernancePolicy& policy,
    const GovernanceProposalEnvelope& envelope,
    const GovernanceDecisionRecord& decision
) {
    // 1. Validate each input individually.
    if (!policy.isValid()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::INVALID_POLICY,
            "GovernanceApprovalBridge: policy is invalid: " + policy.rejectionReason()
        );
    }

    if (!envelope.isValid()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::INVALID_ENVELOPE,
            "GovernanceApprovalBridge: envelope is invalid: " + envelope.rejectionReason()
        );
    }

    if (!decision.isValid()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::INVALID_DECISION,
            "GovernanceApprovalBridge: decision is invalid: " + decision.rejectionReason()
        );
    }

    // 2. Policy versions must be consistent across all three inputs.
    if (envelope.governancePolicyVersion() != policy.policyVersion()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH,
            "GovernanceApprovalBridge: envelope.governancePolicyVersion='" +
            envelope.governancePolicyVersion() +
            "' does not match policy.policyVersion='" + policy.policyVersion() + "'."
        );
    }

    if (decision.policyVersion() != policy.policyVersion()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH,
            "GovernanceApprovalBridge: decision.policyVersion='" +
            decision.policyVersion() +
            "' does not match policy.policyVersion='" + policy.policyVersion() + "'."
        );
    }

    // 3. The decision must reference the same governance proposal as the envelope.
    if (decision.governanceProposalId() != envelope.governanceProposalId()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::PROPOSAL_MISMATCH,
            "GovernanceApprovalBridge: decision.governanceProposalId='" +
            decision.governanceProposalId() +
            "' does not match envelope.governanceProposalId='" +
            envelope.governanceProposalId() + "'."
        );
    }

    if (decision.proposalType() != envelope.proposalType()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::PROPOSAL_MISMATCH,
            "GovernanceApprovalBridge: decision.proposalType='" +
            decision.proposalType() +
            "' does not match envelope.proposalType='" + envelope.proposalType() + "'."
        );
    }

    // 4. Only APPROVED decisions may produce a TreasuryApproval.
    if (decision.decisionStatus() != GovernanceDecisionStatus::APPROVED) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::DECISION_NOT_APPROVED,
            "GovernanceApprovalBridge: decision status is " +
            governanceDecisionStatusToString(decision.decisionStatus()) +
            ", must be APPROVED."
        );
    }

    // 5. Review period: the decision must have been made after enough blocks elapsed.
    const std::uint64_t earliestDecisionBlock =
        envelope.submittedAtBlock() + policy.reviewPeriodBlocks();

    if (decision.decidedAtBlock() < earliestDecisionBlock) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::REVIEW_PERIOD_NOT_SATISFIED,
            "GovernanceApprovalBridge: decision.decidedAtBlock=" +
            std::to_string(decision.decidedAtBlock()) +
            " is earlier than the required " +
            std::to_string(earliestDecisionBlock) +
            " (submittedAtBlock=" + std::to_string(envelope.submittedAtBlock()) +
            " + reviewPeriodBlocks=" + std::to_string(policy.reviewPeriodBlocks()) + ")."
        );
    }

    // 6. When the policy requires a decision proof, the decision must carry one.
    if (policy.requireDecisionProof() && decision.decisionProof().empty()) {
        return GovernanceApprovalBridgeResult::rejected(
            GovernanceApprovalBridgeStatus::DECISION_PROOF_REQUIRED,
            "GovernanceApprovalBridge: policy requires a decisionProof but decision carries none."
        );
    }

    // 7. Build deterministic proof and produce TreasuryApproval.
    const std::string approvalProof = TreasuryApprovalProof::build(
        envelope.governanceProposalId(),
        envelope.treasuryProposal().proposalId(),
        decision.decisionId(),
        policy.policyVersion(),
        decision.decidedAtBlock()
    );

    const std::string approvalId = "gov-approval:" + decision.decisionId();

    TreasuryApproval approval(
        approvalId,
        envelope.treasuryProposal().proposalId(),
        decision.decidedAtBlock(),
        decision.decisionMaker(),
        approvalProof
    );

    return GovernanceApprovalBridgeResult::accepted(std::move(approval));
}

} // namespace nodo::economics
