#include "node/FinalizedTreasuryExecutionAudit.hpp"

#include <set>
#include <utility>

namespace nodo::node {

std::string treasuryExecutionAuditStatusToString(TreasuryExecutionAuditStatus status) {
    switch (status) {
        case TreasuryExecutionAuditStatus::ACCEPTED:
            return "ACCEPTED";
        case TreasuryExecutionAuditStatus::DUPLICATE_PROPOSAL_ID:
            return "DUPLICATE_PROPOSAL_ID";
        case TreasuryExecutionAuditStatus::DUPLICATE_APPROVAL_ID:
            return "DUPLICATE_APPROVAL_ID";
        case TreasuryExecutionAuditStatus::DUPLICATE_SPEND_ID:
            return "DUPLICATE_SPEND_ID";
        case TreasuryExecutionAuditStatus::DUPLICATE_EVIDENCE_ID:
            return "DUPLICATE_EVIDENCE_ID";
        case TreasuryExecutionAuditStatus::INVALID_EVIDENCE:
            return "INVALID_EVIDENCE";
        default:
            return "UNKNOWN";
    }
}

TreasuryExecutionAuditResult::TreasuryExecutionAuditResult()
    : m_accepted(false),
      m_status(TreasuryExecutionAuditStatus::INVALID_EVIDENCE),
      m_reason(""),
      m_evidenceCount(0),
      m_failedAtIndex(0) {}

TreasuryExecutionAuditResult TreasuryExecutionAuditResult::accepted(
    std::size_t evidenceCount
) {
    TreasuryExecutionAuditResult r;
    r.m_accepted = true;
    r.m_status = TreasuryExecutionAuditStatus::ACCEPTED;
    r.m_evidenceCount = evidenceCount;
    return r;
}

TreasuryExecutionAuditResult TreasuryExecutionAuditResult::rejected(
    TreasuryExecutionAuditStatus status,
    std::string reason,
    std::size_t failedAtIndex
) {
    TreasuryExecutionAuditResult r;
    r.m_accepted = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    r.m_failedAtIndex = failedAtIndex;
    return r;
}

bool TreasuryExecutionAuditResult::accepted() const { return m_accepted; }
TreasuryExecutionAuditStatus TreasuryExecutionAuditResult::status() const { return m_status; }
const std::string& TreasuryExecutionAuditResult::reason() const { return m_reason; }
std::size_t TreasuryExecutionAuditResult::evidenceCount() const { return m_evidenceCount; }
std::size_t TreasuryExecutionAuditResult::failedAtIndex() const { return m_failedAtIndex; }

TreasuryExecutionAuditResult FinalizedTreasuryExecutionAudit::auditEvidence(
    const std::vector<economics::TreasuryExecutionEvidence>& evidenceList
) {
    std::set<std::string> evidenceIds;
    std::set<std::string> proposalIds;
    std::set<std::string> approvalIds;
    std::set<std::string> spendIds;

    for (std::size_t i = 0; i < evidenceList.size(); ++i) {
        const auto& evidence = evidenceList[i];

        if (!evidence.isValid()) {
            return TreasuryExecutionAuditResult::rejected(
                TreasuryExecutionAuditStatus::INVALID_EVIDENCE,
                "FinalizedTreasuryExecutionAudit: evidence[" + std::to_string(i) +
                "] is invalid: " + evidence.rejectionReason(),
                i
            );
        }

        if (!evidenceIds.insert(evidence.evidenceId()).second) {
            return TreasuryExecutionAuditResult::rejected(
                TreasuryExecutionAuditStatus::DUPLICATE_EVIDENCE_ID,
                "FinalizedTreasuryExecutionAudit: duplicate evidenceId='" +
                evidence.evidenceId() + "' at index " + std::to_string(i) + ".",
                i
            );
        }

        if (!proposalIds.insert(evidence.proposal().proposalId()).second) {
            return TreasuryExecutionAuditResult::rejected(
                TreasuryExecutionAuditStatus::DUPLICATE_PROPOSAL_ID,
                "FinalizedTreasuryExecutionAudit: duplicate proposalId='" +
                evidence.proposal().proposalId() + "' at index " + std::to_string(i) + ".",
                i
            );
        }

        if (!approvalIds.insert(evidence.approval().approvalId()).second) {
            return TreasuryExecutionAuditResult::rejected(
                TreasuryExecutionAuditStatus::DUPLICATE_APPROVAL_ID,
                "FinalizedTreasuryExecutionAudit: duplicate approvalId='" +
                evidence.approval().approvalId() + "' at index " + std::to_string(i) + ".",
                i
            );
        }

        if (!spendIds.insert(evidence.spendRecord().spendId()).second) {
            return TreasuryExecutionAuditResult::rejected(
                TreasuryExecutionAuditStatus::DUPLICATE_SPEND_ID,
                "FinalizedTreasuryExecutionAudit: duplicate spendId='" +
                evidence.spendRecord().spendId() + "' at index " + std::to_string(i) + ".",
                i
            );
        }
    }

    return TreasuryExecutionAuditResult::accepted(evidenceList.size());
}

} // namespace nodo::node
