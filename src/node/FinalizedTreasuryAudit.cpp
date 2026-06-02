#include "node/FinalizedTreasuryAudit.hpp"

#include "node/FinalizedTreasurySectionValidator.hpp"

#include <utility>

namespace nodo::node {

FinalizedTreasuryAuditResult::FinalizedTreasuryAuditResult()
    : m_passed(false),
      m_reason("Uninitialized."),
      m_totalSpendRecords(0),
      m_failedAtArtifactIndex(0) {}

FinalizedTreasuryAuditResult FinalizedTreasuryAuditResult::passed(
    economics::EpochTreasuryReport rebuiltReport,
    std::size_t totalSpendRecords
) {
    FinalizedTreasuryAuditResult r;
    r.m_passed = true;
    r.m_reason = "";
    r.m_rebuiltReport = std::move(rebuiltReport);
    r.m_totalSpendRecords = totalSpendRecords;
    r.m_failedAtArtifactIndex = 0;
    return r;
}

FinalizedTreasuryAuditResult FinalizedTreasuryAuditResult::failed(
    std::string reason,
    std::uint64_t failedAtArtifactIndex
) {
    FinalizedTreasuryAuditResult r;
    r.m_passed = false;
    r.m_reason = std::move(reason);
    r.m_failedAtArtifactIndex = failedAtArtifactIndex;
    return r;
}

bool FinalizedTreasuryAuditResult::passed() const { return m_passed; }
const std::string& FinalizedTreasuryAuditResult::reason() const { return m_reason; }
const economics::EpochTreasuryReport& FinalizedTreasuryAuditResult::rebuiltReport() const {
    return m_rebuiltReport;
}
std::size_t FinalizedTreasuryAuditResult::totalSpendRecords() const {
    return m_totalSpendRecords;
}
std::uint64_t FinalizedTreasuryAuditResult::failedAtArtifactIndex() const {
    return m_failedAtArtifactIndex;
}

FinalizedTreasuryAuditResult FinalizedTreasuryAudit::auditArtifacts(
    std::uint64_t epoch,
    const std::vector<FinalizedBlockArtifact>& artifacts
) {
    // Collect evidence and spend records from all artifacts.
    std::vector<economics::TreasuryExecutionEvidence> allEvidence;
    std::vector<economics::TreasurySpendRecord> allSpends;
    bool anyEvidence = false;

    for (std::size_t i = 0; i < artifacts.size(); ++i) {
        const auto& section = artifacts[i].treasurySection();
        const auto validation = FinalizedTreasurySectionValidator::validate(section);

        if (!validation.passed()) {
            return FinalizedTreasuryAuditResult::failed(
                "FinalizedTreasuryAudit: artifact at index " +
                std::to_string(i) + " has invalid treasury section: " +
                validation.reason(),
                static_cast<std::uint64_t>(i)
            );
        }

        if (section.hasEvidence()) {
            anyEvidence = true;
            for (const auto& ev : section.executionEvidence()) {
                allEvidence.push_back(ev);
                allSpends.push_back(ev.spendRecord());
            }
        } else {
            // Only empty sections reach this branch; non-empty spend-only
            // sections are rejected by FinalizedTreasurySectionValidator.
            for (const auto& rec : section.spendRecords()) {
                allSpends.push_back(rec);
            }
        }
    }

    // If any evidence was present, run replay protection across all evidence.
    if (anyEvidence && !allEvidence.empty()) {
        const auto replayResult =
            FinalizedTreasuryExecutionAudit::auditEvidence(allEvidence);

        if (!replayResult.accepted()) {
            return FinalizedTreasuryAuditResult::failed(
                "FinalizedTreasuryAudit: replay protection rejected evidence: " +
                replayResult.reason(),
                static_cast<std::uint64_t>(replayResult.failedAtIndex())
            );
        }
    }

    const economics::EpochTreasuryReport rebuiltReport =
        economics::EpochTreasuryReport::fromSpendRecords(epoch, allSpends);

    if (!rebuiltReport.isValid()) {
        return FinalizedTreasuryAuditResult::failed(
            "FinalizedTreasuryAudit: cannot rebuild epoch treasury report: " +
            rebuiltReport.rejectionReason()
        );
    }

    return FinalizedTreasuryAuditResult::passed(
        rebuiltReport,
        allSpends.size()
    );
}

} // namespace nodo::node
