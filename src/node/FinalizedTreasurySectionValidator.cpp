#include "node/FinalizedTreasurySectionValidator.hpp"

#include "economics/TreasuryGovernanceEvidenceValidator.hpp"

#include <utility>

namespace nodo::node {

std::string treasurySectionValidationStatusToString(
    TreasurySectionValidationStatus status
) {
    switch (status) {
        case TreasurySectionValidationStatus::VALID:
            return "VALID";
        case TreasurySectionValidationStatus::INVALID_SECTION:
            return "INVALID_SECTION";
        case TreasurySectionValidationStatus::INVALID_SPEND_RECORD:
            return "INVALID_SPEND_RECORD";
        case TreasurySectionValidationStatus::SPEND_WITHOUT_EVIDENCE:
            return "SPEND_WITHOUT_EVIDENCE";
        case TreasurySectionValidationStatus::INVALID_EVIDENCE:
            return "INVALID_EVIDENCE";
        case TreasurySectionValidationStatus::EVIDENCE_SPEND_MISMATCH:
            return "EVIDENCE_SPEND_MISMATCH";
        case TreasurySectionValidationStatus::MISSING_GOVERNANCE_CONTEXT:
            return "MISSING_GOVERNANCE_CONTEXT";
        case TreasurySectionValidationStatus::INVALID_GOVERNANCE_CONTEXT:
            return "INVALID_GOVERNANCE_CONTEXT";
        default:
            return "UNKNOWN";
    }
}

TreasurySectionValidationResult::TreasurySectionValidationResult()
    : m_status(TreasurySectionValidationStatus::INVALID_SECTION),
      m_reason("Uninitialized.") {}

TreasurySectionValidationResult TreasurySectionValidationResult::valid() {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::VALID;
    r.m_reason = "";
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidSection(
    std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_SECTION;
    r.m_reason = std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidSpendRecord(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_SPEND_RECORD;
    r.m_reason = "spend record at index " + std::to_string(index) +
                 " is invalid: " + std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::spendWithoutEvidence(
    std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::SPEND_WITHOUT_EVIDENCE;
    r.m_reason = std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidEvidence(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_EVIDENCE;
    r.m_reason = "evidence at index " + std::to_string(index) +
                 " is invalid: " + std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::evidenceSpendMismatch(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::EVIDENCE_SPEND_MISMATCH;
    r.m_reason = "evidence/spend mismatch at index " + std::to_string(index) +
                 ": " + std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::missingGovernanceContext(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::MISSING_GOVERNANCE_CONTEXT;
    r.m_reason = "evidence at index " + std::to_string(index) +
                 " is missing governance context: " + std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidGovernanceContext(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_GOVERNANCE_CONTEXT;
    r.m_reason = "evidence at index " + std::to_string(index) +
                 " has invalid governance context: " + std::move(reason);
    return r;
}

bool TreasurySectionValidationResult::passed() const {
    return m_status == TreasurySectionValidationStatus::VALID;
}

TreasurySectionValidationStatus TreasurySectionValidationResult::status() const {
    return m_status;
}

const std::string& TreasurySectionValidationResult::reason() const {
    return m_reason;
}

TreasurySectionValidationResult FinalizedTreasurySectionValidator::validate(
    const FinalizedTreasurySection& section
) {
    if (!section.isValid()) {
        return TreasurySectionValidationResult::invalidSection(
            section.rejectionReason()
        );
    }

    // Empty section is always valid.
    if (section.spendRecordCount() == 0 && section.evidenceCount() == 0) {
        return TreasurySectionValidationResult::valid();
    }

    // Non-empty section with spend records but no evidence is rejected.
    // Evidence is canonical. Spend records without evidence cannot be verified.
    if (section.spendRecordCount() > 0 && !section.hasEvidence()) {
        return TreasurySectionValidationResult::spendWithoutEvidence(
            "FinalizedTreasurySectionValidator: section has " +
            std::to_string(section.spendRecordCount()) +
            " spend record(s) but no execution evidence. "
            "Non-empty treasury sections must be built from execution evidence."
        );
    }

    // Validate each evidence entry: structural validity and governance context.
    for (std::size_t i = 0; i < section.executionEvidence().size(); ++i) {
        const auto& ev = section.executionEvidence()[i];
        if (!ev.isValid()) {
            return TreasurySectionValidationResult::invalidEvidence(
                i, ev.rejectionReason()
            );
        }

        // Production evidence must carry governance approval context proving
        // the TreasuryApproval was produced by GovernanceApprovalBridge.
        const auto govResult =
            economics::TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);

        if (govResult.status() ==
            economics::GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT) {
            return TreasurySectionValidationResult::missingGovernanceContext(
                i, govResult.reason()
            );
        }

        if (!govResult.isAccepted()) {
            return TreasurySectionValidationResult::invalidGovernanceContext(
                i, govResult.reason()
            );
        }
    }

    // Evidence count must equal spend record count (evidence derives spendRecords).
    if (section.evidenceCount() != section.spendRecordCount()) {
        return TreasurySectionValidationResult::evidenceSpendMismatch(
            0,
            "evidence count " + std::to_string(section.evidenceCount()) +
            " does not match spend record count " +
            std::to_string(section.spendRecordCount())
        );
    }

    // Verify each spend record matches the corresponding evidence.
    for (std::size_t i = 0; i < section.executionEvidence().size(); ++i) {
        const auto& ev = section.executionEvidence()[i];
        const auto& rec = section.spendRecords()[i];
        if (ev.spendRecord().spendId() != rec.spendId()) {
            return TreasurySectionValidationResult::evidenceSpendMismatch(
                i,
                "evidence[" + std::to_string(i) + "].spendRecord.spendId='" +
                ev.spendRecord().spendId() +
                "' does not match spendRecords[" + std::to_string(i) +
                "].spendId='" + rec.spendId() + "'."
            );
        }
    }

    return TreasurySectionValidationResult::valid();
}

} // namespace nodo::node
