#ifndef NODO_NODE_FINALIZED_TREASURY_AUDIT_HPP
#define NODO_NODE_FINALIZED_TREASURY_AUDIT_HPP

#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/FinalizedBlockArtifact.hpp"

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
 * FinalizedTreasuryAudit collects and validates TreasurySpendRecord entries
 * from a sequence of finalized artifacts, then derives an EpochTreasuryReport.
 *
 * Security principle:
 * No treasury spend can hide from the audit. Every finalized artifact's
 * treasury section must be valid, and the rebuilt EpochTreasuryReport must
 * agree with any persisted treasury report.
 */
class FinalizedTreasuryAudit {
public:
    // Audit the treasury sections of a sequence of finalized artifacts.
    // Returns the rebuilt EpochTreasuryReport derived from all spend records.
    static FinalizedTreasuryAuditResult auditArtifacts(
        std::uint64_t epoch,
        const std::vector<FinalizedBlockArtifact>& artifacts
    );
};

} // namespace nodo::node

#endif
