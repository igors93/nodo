#ifndef NODO_NODE_TREASURY_REPORT_DERIVER_HPP
#define NODO_NODE_TREASURY_REPORT_DERIVER_HPP

#include "economics/EpochTreasuryReport.hpp"
#include "node/FinalizedBlockArtifact.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

enum class TreasuryReportDerivationStatus {
    DERIVED,
    NO_ARTIFACTS,
    ARTIFACT_VALIDATION_FAILED,
    REPORT_MISMATCH,
    EVIDENCE_MISSING
};

std::string treasuryReportDerivationStatusToString(
    TreasuryReportDerivationStatus status
);

class TreasuryReportDerivationResult {
public:
    TreasuryReportDerivationResult();

    static TreasuryReportDerivationResult derived(
        economics::EpochTreasuryReport report
    );

    static TreasuryReportDerivationResult noArtifacts(
        economics::EpochTreasuryReport emptyReport
    );

    static TreasuryReportDerivationResult failed(
        TreasuryReportDerivationStatus status,
        std::string reason
    );

    bool isDerived() const;
    TreasuryReportDerivationStatus status() const;
    const std::string& reason() const;
    const economics::EpochTreasuryReport& report() const;

private:
    TreasuryReportDerivationStatus m_status;
    std::string m_reason;
    economics::EpochTreasuryReport m_report;
};

/*
 * TreasuryReportDeriver produces canonical EpochTreasuryReport records from
 * finalized artifact sequences. It is the single entry point for deriving
 * treasury spend summaries from on-chain history.
 *
 * Security principle:
 * Treasury reports are never manually constructed. They are always derived
 * from the finalized artifacts they summarize. Any divergence between the
 * derived report and a stored report must fail audit.
 *
 * An empty artifact sequence or a sequence with no treasury spends produces a
 * valid empty report with zero totals. This is not a failure — it is the
 * canonical representation of zero treasury activity.
 */
class TreasuryReportDeriver {
public:
    // Derive an EpochTreasuryReport from a sequence of finalized artifacts.
    static TreasuryReportDerivationResult deriveFromArtifacts(
        std::uint64_t epoch,
        const std::vector<FinalizedBlockArtifact>& artifacts
    );

    // Verify that a persisted report matches the report derived from artifacts.
    // Returns true when the persisted report is consistent with finalized history.
    static bool verifyConsistency(
        const economics::EpochTreasuryReport& persisted,
        const economics::EpochTreasuryReport& derived
    );
};

} // namespace nodo::node

#endif
