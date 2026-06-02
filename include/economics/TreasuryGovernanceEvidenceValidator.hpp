#ifndef NODO_ECONOMICS_TREASURY_GOVERNANCE_EVIDENCE_VALIDATOR_HPP
#define NODO_ECONOMICS_TREASURY_GOVERNANCE_EVIDENCE_VALIDATOR_HPP

#include "economics/TreasuryExecutionEvidence.hpp"

#include <string>

namespace nodo::economics {

enum class GovernanceEvidenceValidationStatus {
    ACCEPTED,
    MISSING_GOVERNANCE_CONTEXT,
    INVALID_GOVERNANCE_POLICY,
    INVALID_GOVERNANCE_ENVELOPE,
    INVALID_GOVERNANCE_DECISION,
    POLICY_VERSION_MISMATCH,
    PROPOSAL_MISMATCH,
    DECISION_NOT_APPROVED,
    REVIEW_PERIOD_NOT_SATISFIED,
    DECISION_PROOF_REQUIRED,
    INVALID_GOVERNANCE_LIFECYCLE,
    APPROVAL_PROOF_MISMATCH,
    APPROVAL_ID_MISMATCH,
    APPROVAL_APPROVER_MISMATCH,
    APPROVAL_PROPOSAL_MISMATCH
};

std::string governanceEvidenceValidationStatusToString(
    GovernanceEvidenceValidationStatus status
);

class GovernanceEvidenceValidationResult {
public:
    GovernanceEvidenceValidationResult();

    static GovernanceEvidenceValidationResult accepted();
    static GovernanceEvidenceValidationResult rejected(
        GovernanceEvidenceValidationStatus status,
        std::string reason
    );

    bool isAccepted() const;
    GovernanceEvidenceValidationStatus status() const;
    const std::string& reason() const;

private:
    GovernanceEvidenceValidationStatus m_status;
    std::string m_reason;
};

/*
 * TreasuryGovernanceEvidenceValidator verifies that the TreasuryApproval
 * embedded in evidence was produced from a verified governance lifecycle and
 * not forged directly.
 *
 * Security principle:
 * The validator re-runs GovernanceApprovalBridge with the lifecycle stored in
 * evidence and compares the reproduced TreasuryApproval against the actual
 * approval. A mismatch on any field proves the approval was not produced by
 * vote-derived governance evidence.
 *
 * Checks performed:
 *   - evidence carries a GovernanceApprovalContext with a lifecycle;
 *   - the bridge accepts the verified lifecycle;
 *   - the reproduced approval matches the stored approval on all fields:
 *     approvalId, proposalId, approvedAtBlock, approver, approvalProof.
 */
class TreasuryGovernanceEvidenceValidator {
public:
    static GovernanceEvidenceValidationResult validateGovernanceContext(
        const TreasuryExecutionEvidence& evidence
    );
};

} // namespace nodo::economics

#endif
