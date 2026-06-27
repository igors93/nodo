#include "economics/TreasuryGovernanceEvidenceValidator.hpp"

#include "economics/GovernanceApprovalBridge.hpp"

#include <utility>

namespace nodo::economics {

std::string governanceEvidenceValidationStatusToString(
    GovernanceEvidenceValidationStatus status
) {
    switch (status) {
        case GovernanceEvidenceValidationStatus::ACCEPTED:
            return "ACCEPTED";
        case GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT:
            return "MISSING_GOVERNANCE_CONTEXT";
        case GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_POLICY:
            return "INVALID_GOVERNANCE_POLICY";
        case GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_ENVELOPE:
            return "INVALID_GOVERNANCE_ENVELOPE";
        case GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_DECISION:
            return "INVALID_GOVERNANCE_DECISION";
        case GovernanceEvidenceValidationStatus::POLICY_VERSION_MISMATCH:
            return "POLICY_VERSION_MISMATCH";
        case GovernanceEvidenceValidationStatus::PROPOSAL_MISMATCH:
            return "PROPOSAL_MISMATCH";
        case GovernanceEvidenceValidationStatus::DECISION_NOT_APPROVED:
            return "DECISION_NOT_APPROVED";
        case GovernanceEvidenceValidationStatus::REVIEW_PERIOD_NOT_SATISFIED:
            return "REVIEW_PERIOD_NOT_SATISFIED";
        case GovernanceEvidenceValidationStatus::DECISION_PROOF_REQUIRED:
            return "DECISION_PROOF_REQUIRED";
        case GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_LIFECYCLE:
            return "INVALID_GOVERNANCE_LIFECYCLE";
        case GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH:
            return "APPROVAL_PROOF_MISMATCH";
        case GovernanceEvidenceValidationStatus::APPROVAL_ID_MISMATCH:
            return "APPROVAL_ID_MISMATCH";
        case GovernanceEvidenceValidationStatus::APPROVAL_APPROVER_MISMATCH:
            return "APPROVAL_APPROVER_MISMATCH";
        case GovernanceEvidenceValidationStatus::APPROVAL_PROPOSAL_MISMATCH:
            return "APPROVAL_PROPOSAL_MISMATCH";
        default:
            return "UNKNOWN";
    }
}

GovernanceEvidenceValidationResult::GovernanceEvidenceValidationResult()
    : m_status(GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT),
      m_reason("") {}

GovernanceEvidenceValidationResult GovernanceEvidenceValidationResult::accepted() {
    GovernanceEvidenceValidationResult r;
    r.m_status = GovernanceEvidenceValidationStatus::ACCEPTED;
    return r;
}

GovernanceEvidenceValidationResult GovernanceEvidenceValidationResult::rejected(
    GovernanceEvidenceValidationStatus status,
    std::string reason
) {
    GovernanceEvidenceValidationResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool GovernanceEvidenceValidationResult::isAccepted() const {
    return m_status == GovernanceEvidenceValidationStatus::ACCEPTED;
}
GovernanceEvidenceValidationStatus GovernanceEvidenceValidationResult::status() const {
    return m_status;
}
const std::string& GovernanceEvidenceValidationResult::reason() const { return m_reason; }

static GovernanceEvidenceValidationStatus mapBridgeStatus(
    GovernanceApprovalBridgeStatus s
) {
    switch (s) {
        case GovernanceApprovalBridgeStatus::INVALID_POLICY:
            return GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_POLICY;
        case GovernanceApprovalBridgeStatus::INVALID_ENVELOPE:
            return GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_ENVELOPE;
        case GovernanceApprovalBridgeStatus::INVALID_DECISION:
            return GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_DECISION;
        case GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH:
            return GovernanceEvidenceValidationStatus::POLICY_VERSION_MISMATCH;
        case GovernanceApprovalBridgeStatus::PROPOSAL_MISMATCH:
            return GovernanceEvidenceValidationStatus::PROPOSAL_MISMATCH;
        case GovernanceApprovalBridgeStatus::DECISION_NOT_APPROVED:
            return GovernanceEvidenceValidationStatus::DECISION_NOT_APPROVED;
        case GovernanceApprovalBridgeStatus::REVIEW_PERIOD_NOT_SATISFIED:
            return GovernanceEvidenceValidationStatus::REVIEW_PERIOD_NOT_SATISFIED;
        case GovernanceApprovalBridgeStatus::DECISION_PROOF_REQUIRED:
            return GovernanceEvidenceValidationStatus::DECISION_PROOF_REQUIRED;
        case GovernanceApprovalBridgeStatus::INVALID_LIFECYCLE:
            return GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_LIFECYCLE;
        default:
            return GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT;
    }
}

GovernanceEvidenceValidationResult TreasuryGovernanceEvidenceValidator::validateGovernanceContext(
    const TreasuryExecutionEvidence& evidence
) {
    // Evidence must carry governance context.
    if (!evidence.hasGovernanceContext()) {
        return GovernanceEvidenceValidationResult::rejected(
            GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT,
            "TreasuryGovernanceEvidenceValidator: evidence does not carry a "
            "GovernanceApprovalContext. Production treasury evidence must be "
            "produced via GovernanceApprovalBridge."
        );
    }

    const auto& ctx = evidence.governanceContext();

    // Re-run the lifecycle bridge to reproduce the expected TreasuryApproval.
    const auto bridgeResult =
        GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle(
            ctx.governanceLifecycle
        );

    if (!bridgeResult.isAccepted()) {
        return GovernanceEvidenceValidationResult::rejected(
            mapBridgeStatus(bridgeResult.status()),
            "TreasuryGovernanceEvidenceValidator: bridge rejected governance context: " +
            bridgeResult.reason()
        );
    }

    const TreasuryApproval& reproduced = bridgeResult.treasuryApproval();
    const TreasuryApproval& actual    = evidence.approval();

    // The reproduced approval must match the stored approval on every field.
    if (reproduced.approvalProof() != actual.approvalProof()) {
        return GovernanceEvidenceValidationResult::rejected(
            GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH,
            "TreasuryGovernanceEvidenceValidator: approval proof mismatch: "
            "reproduced='" + reproduced.approvalProof() +
            "' stored='" + actual.approvalProof() + "'."
        );
    }

    if (reproduced.approvalId() != actual.approvalId()) {
        return GovernanceEvidenceValidationResult::rejected(
            GovernanceEvidenceValidationStatus::APPROVAL_ID_MISMATCH,
            "TreasuryGovernanceEvidenceValidator: approvalId mismatch: "
            "reproduced='" + reproduced.approvalId() +
            "' stored='" + actual.approvalId() + "'."
        );
    }

    if (reproduced.proposalId() != actual.proposalId()) {
        return GovernanceEvidenceValidationResult::rejected(
            GovernanceEvidenceValidationStatus::APPROVAL_PROPOSAL_MISMATCH,
            "TreasuryGovernanceEvidenceValidator: approval proposalId mismatch: "
            "reproduced='" + reproduced.proposalId() +
            "' stored='" + actual.proposalId() + "'."
        );
    }

    if (reproduced.approver() != actual.approver()) {
        return GovernanceEvidenceValidationResult::rejected(
            GovernanceEvidenceValidationStatus::APPROVAL_APPROVER_MISMATCH,
            "TreasuryGovernanceEvidenceValidator: approver mismatch: "
            "reproduced='" + reproduced.approver() +
            "' stored='" + actual.approver() + "'."
        );
    }

    return GovernanceEvidenceValidationResult::accepted();
}

} // namespace nodo::economics
