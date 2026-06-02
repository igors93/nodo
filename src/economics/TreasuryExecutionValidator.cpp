#include "economics/TreasuryExecutionValidator.hpp"

#include "economics/TreasuryGovernanceEvidenceValidator.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <utility>

namespace nodo::economics {

std::string treasuryExecutionValidationStatusToString(
    TreasuryExecutionValidationStatus status
) {
    switch (status) {
        case TreasuryExecutionValidationStatus::ACCEPTED:
            return "ACCEPTED";
        case TreasuryExecutionValidationStatus::INVALID_EVIDENCE:
            return "INVALID_EVIDENCE";
        case TreasuryExecutionValidationStatus::SPEND_VALIDATOR_REJECTED:
            return "SPEND_VALIDATOR_REJECTED";
        case TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH:
            return "SPEND_RECORD_MISMATCH";
        case TreasuryExecutionValidationStatus::MISSING_GOVERNANCE_CONTEXT:
            return "MISSING_GOVERNANCE_CONTEXT";
        case TreasuryExecutionValidationStatus::INVALID_GOVERNANCE_CONTEXT:
            return "INVALID_GOVERNANCE_CONTEXT";
        default:
            return "UNKNOWN";
    }
}

TreasuryExecutionValidationResult::TreasuryExecutionValidationResult()
    : m_status(TreasuryExecutionValidationStatus::INVALID_EVIDENCE),
      m_reason("") {}

TreasuryExecutionValidationResult TreasuryExecutionValidationResult::accepted() {
    TreasuryExecutionValidationResult r;
    r.m_status = TreasuryExecutionValidationStatus::ACCEPTED;
    return r;
}

TreasuryExecutionValidationResult TreasuryExecutionValidationResult::rejected(
    TreasuryExecutionValidationStatus status,
    std::string reason
) {
    TreasuryExecutionValidationResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool TreasuryExecutionValidationResult::isAccepted() const {
    return m_status == TreasuryExecutionValidationStatus::ACCEPTED;
}
TreasuryExecutionValidationStatus TreasuryExecutionValidationResult::status() const {
    return m_status;
}
const std::string& TreasuryExecutionValidationResult::reason() const { return m_reason; }

TreasuryExecutionValidationResult TreasuryExecutionValidator::validateEvidence(
    const TreasuryExecutionEvidence& evidence
) {
    // 1. Structural validity of the evidence (field-level checks + consistency).
    if (!evidence.isValid()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::INVALID_EVIDENCE,
            "TreasuryExecutionValidator: evidence is structurally invalid: " +
            evidence.rejectionReason()
        );
    }

    // 2. Recompute the spend through TreasurySpendValidator.
    const TreasurySpendValidationResult spendResult =
        TreasurySpendValidator::validateSpend(
            evidence.treasuryAccountBefore(),
            evidence.policy(),
            evidence.proposal(),
            evidence.approval(),
            evidence.currentBlockHeight(),
            evidence.epochSpentSoFar()
        );

    if (!spendResult.accepted()) {  // TreasurySpendValidationResult::accepted() is not renamed
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_VALIDATOR_REJECTED,
            "TreasuryExecutionValidator: TreasurySpendValidator rejected the spend: " +
            spendResult.reason()
        );
    }

    // 3. Compare the recomputed spend record with the stored spend record.
    const TreasurySpendRecord& computed = spendResult.spendRecord();
    const TreasurySpendRecord& stored = evidence.spendRecord();

    if (computed.spendId() != stored.spendId()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: spendId mismatch: computed='" +
            computed.spendId() + "' stored='" + stored.spendId() + "'."
        );
    }
    if (computed.proposalId() != stored.proposalId()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: proposalId mismatch."
        );
    }
    if (computed.recipientAddress() != stored.recipientAddress()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: recipientAddress mismatch."
        );
    }
    if (computed.amount() != stored.amount()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: amount mismatch: computed=" +
            std::to_string(computed.amount().rawUnits()) +
            " stored=" + std::to_string(stored.amount().rawUnits()) + "."
        );
    }
    if (computed.purpose() != stored.purpose()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: purpose mismatch."
        );
    }
    if (computed.executedAtBlock() != stored.executedAtBlock()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: executedAtBlock mismatch."
        );
    }
    if (computed.epoch() != stored.epoch()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: epoch mismatch."
        );
    }
    if (computed.treasuryBalanceBefore() != stored.treasuryBalanceBefore()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: treasuryBalanceBefore mismatch."
        );
    }
    if (computed.treasuryBalanceAfter() != stored.treasuryBalanceAfter()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH,
            "TreasuryExecutionValidator: treasuryBalanceAfter mismatch."
        );
    }

    // 4. Validate governance context: the approval must have been produced by
    //    GovernanceApprovalBridge, not forged directly.
    const GovernanceEvidenceValidationResult govResult =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);

    if (govResult.status() == GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::MISSING_GOVERNANCE_CONTEXT,
            "TreasuryExecutionValidator: evidence is missing governance approval context. "
            "Production treasury evidence must be produced via GovernanceApprovalBridge: " +
            govResult.reason()
        );
    }

    if (!govResult.isAccepted()) {
        return TreasuryExecutionValidationResult::rejected(
            TreasuryExecutionValidationStatus::INVALID_GOVERNANCE_CONTEXT,
            "TreasuryExecutionValidator: governance context validation failed: " +
            govResult.reason()
        );
    }

    return TreasuryExecutionValidationResult::accepted();
}

} // namespace nodo::economics
