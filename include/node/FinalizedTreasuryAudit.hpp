#ifndef NODO_NODE_FINALIZED_TREASURY_AUDIT_HPP
#define NODO_NODE_FINALIZED_TREASURY_AUDIT_HPP

#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "node/FinalizedTreasuryExecutionAudit.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class FinalizedTreasuryAuditResult {
public:
    FinalizedTreasuryAuditResult();

    static FinalizedTreasuryAuditResult passed(
        economics::EpochTreasuryReport rebuiltReport,
        std::size_t totalSpendRecords
    );

    static FinalizedTreasuryAuditResult failed(
        std::string reason,
        std::uint64_t failedAtArtifactIndex = 0
    );

    bool passed() const;
    const std::string& reason() const;
    const economics::EpochTreasuryReport& rebuiltReport() const;
    std::size_t totalSpendRecords() const;
    std::uint64_t failedAtArtifactIndex() const;

private:
    bool m_passed;
    std::string m_reason;
    economics::EpochTreasuryReport m_rebuiltReport;
    std::size_t m_totalSpendRecords;
    std::uint64_t m_failedAtArtifactIndex;
};

/*
 * FinalizedTreasuryAudit collects and validates treasury execution evidence
 * from a sequence of finalized artifacts, runs replay protection, and derives
 * an EpochTreasuryReport from the validated evidence.
 *
 * Security principle:
 * No treasury spend can hide from the audit. Every finalized artifact's
 * treasury section must be valid, execution evidence must pass replay protection,
 * and the rebuilt EpochTreasuryReport must agree with any persisted report.
 *
 * When evidence is present:
 *   - evidence is validated first;
 *   - replay protection runs across all evidence in the artifact sequence;
 *   - spend records are derived from evidence for the report.
 * Legacy spend-only non-empty sections are rejected by
 * FinalizedTreasurySectionValidator before report rebuild.
 */
class FinalizedTreasuryAudit {
public:
    // Audit the treasury sections of a sequence of finalized artifacts.
    // Returns the rebuilt EpochTreasuryReport derived from validated evidence
    // or (fallback) from spend records when no evidence is available.
    static FinalizedTreasuryAuditResult auditArtifacts(
        std::uint64_t epoch,
        const std::vector<FinalizedBlockArtifact>& artifacts
    );
};

} // namespace nodo::node

#endif
