#ifndef NODO_NODE_FINALIZED_TREASURY_EXECUTION_AUDIT_HPP
#define NODO_NODE_FINALIZED_TREASURY_EXECUTION_AUDIT_HPP

#include "economics/TreasuryExecutionEvidence.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class TreasuryExecutionAuditStatus {
    ACCEPTED,
    DUPLICATE_PROPOSAL_ID,
    DUPLICATE_APPROVAL_ID,
    DUPLICATE_SPEND_ID,
    DUPLICATE_EVIDENCE_ID,
    DUPLICATE_LIFECYCLE_ID,
    DUPLICATE_GOVERNANCE_PROPOSAL_ID,
    INVALID_EVIDENCE
};

std::string treasuryExecutionAuditStatusToString(TreasuryExecutionAuditStatus status);

class TreasuryExecutionAuditResult {
public:
    TreasuryExecutionAuditResult();

    static TreasuryExecutionAuditResult accepted(std::size_t evidenceCount);
    static TreasuryExecutionAuditResult rejected(
        TreasuryExecutionAuditStatus status,
        std::string reason,
        std::size_t failedAtIndex = 0
    );

    bool accepted() const;
    TreasuryExecutionAuditStatus status() const;
    const std::string& reason() const;
    std::size_t evidenceCount() const;
    std::size_t failedAtIndex() const;

private:
    bool m_accepted;
    TreasuryExecutionAuditStatus m_status;
    std::string m_reason;
    std::size_t m_evidenceCount;
    std::size_t m_failedAtIndex;
};

/*
 * FinalizedTreasuryExecutionAudit provides replay protection for treasury
 * execution evidence across a finalized block sequence.
 *
 * Security principle:
 * A proposal may only be executed once. An approval may only authorize one
 * execution. A spend ID must be globally unique. An evidence ID must be
 * globally unique. Any replay attempt must be detected and rejected before
 * the artifact is accepted.
 *
 * This audit is stateless: it scans the provided evidence list and rejects on
 * the first duplicate found.
 */
class FinalizedTreasuryExecutionAudit {
public:
    static TreasuryExecutionAuditResult auditEvidence(
        const std::vector<economics::TreasuryExecutionEvidence>& evidenceList
    );
};

} // namespace nodo::node

#endif
