#include "node/TreasuryReportDeriver.hpp"

#include "node/FinalizedTreasuryAudit.hpp"

#include <utility>

namespace nodo::node {

std::string treasuryReportDerivationStatusToString(
    TreasuryReportDerivationStatus status
) {
    switch (status) {
        case TreasuryReportDerivationStatus::DERIVED:
            return "DERIVED";
        case TreasuryReportDerivationStatus::NO_ARTIFACTS:
            return "NO_ARTIFACTS";
        case TreasuryReportDerivationStatus::ARTIFACT_VALIDATION_FAILED:
            return "ARTIFACT_VALIDATION_FAILED";
        case TreasuryReportDerivationStatus::REPORT_MISMATCH:
            return "REPORT_MISMATCH";
        case TreasuryReportDerivationStatus::EVIDENCE_MISSING:
            return "EVIDENCE_MISSING";
        default:
            return "UNKNOWN";
    }
}

TreasuryReportDerivationResult::TreasuryReportDerivationResult()
    : m_status(TreasuryReportDerivationStatus::NO_ARTIFACTS),
      m_reason(""),
      m_report() {}

TreasuryReportDerivationResult TreasuryReportDerivationResult::derived(
    economics::EpochTreasuryReport report
) {
    TreasuryReportDerivationResult r;
    r.m_status = TreasuryReportDerivationStatus::DERIVED;
    r.m_report = std::move(report);
    return r;
}

TreasuryReportDerivationResult TreasuryReportDerivationResult::noArtifacts(
    economics::EpochTreasuryReport emptyReport
) {
    TreasuryReportDerivationResult r;
    r.m_status = TreasuryReportDerivationStatus::NO_ARTIFACTS;
    r.m_report = std::move(emptyReport);
    return r;
}

TreasuryReportDerivationResult TreasuryReportDerivationResult::failed(
    TreasuryReportDerivationStatus status,
    std::string reason
) {
    TreasuryReportDerivationResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool TreasuryReportDerivationResult::isDerived() const {
    return m_status == TreasuryReportDerivationStatus::DERIVED ||
           m_status == TreasuryReportDerivationStatus::NO_ARTIFACTS;
}

TreasuryReportDerivationStatus TreasuryReportDerivationResult::status() const {
    return m_status;
}

const std::string& TreasuryReportDerivationResult::reason() const {
    return m_reason;
}

const economics::EpochTreasuryReport& TreasuryReportDerivationResult::report() const {
    return m_report;
}

// ---- TreasuryReportDeriver ----

TreasuryReportDerivationResult TreasuryReportDeriver::deriveFromArtifacts(
    std::uint64_t epoch,
    const std::vector<FinalizedBlockArtifact>& artifacts
) {
    if (artifacts.empty()) {
        const economics::EpochTreasuryReport emptyReport =
            economics::EpochTreasuryReport::fromSpendRecords(epoch, {});
        return TreasuryReportDerivationResult::noArtifacts(emptyReport);
    }

    const FinalizedTreasuryAuditResult auditResult =
        FinalizedTreasuryAudit::auditArtifacts(epoch, artifacts);

    if (!auditResult.passed()) {
        return TreasuryReportDerivationResult::failed(
            TreasuryReportDerivationStatus::ARTIFACT_VALIDATION_FAILED,
            "treasury audit failed: " + auditResult.reason()
        );
    }

    return TreasuryReportDerivationResult::derived(auditResult.rebuiltReport());
}

bool TreasuryReportDeriver::verifyConsistency(
    const economics::EpochTreasuryReport& persisted,
    const economics::EpochTreasuryReport& derived
) {
    if (persisted.epoch() != derived.epoch()) {
        return false;
    }
    if (persisted.treasurySpendTotal() != derived.treasurySpendTotal()) {
        return false;
    }
    if (persisted.spendRecordCount() != derived.spendRecordCount()) {
        return false;
    }

    // Record-level comparison when both reports have individual records.
    if (persisted.hasSpendRecords() && derived.hasSpendRecords()) {
        const auto& pRecords = persisted.spendRecords();
        const auto& dRecords = derived.spendRecords();

        if (pRecords.size() != dRecords.size()) {
            return false;
        }

        for (std::size_t i = 0; i < pRecords.size(); ++i) {
            const auto& p = pRecords[i];
            const auto& d = dRecords[i];
            if (p.spendId() != d.spendId() ||
                p.proposalId() != d.proposalId() ||
                p.recipientAddress() != d.recipientAddress() ||
                p.amount() != d.amount() ||
                p.executedAtBlock() != d.executedAtBlock() ||
                p.epoch() != d.epoch()) {
                return false;
            }
        }
    } else if (derived.hasSpendRecords()) {
        // The derived report has records. Check for duplicates in the derived set.
        const auto& dRecords = derived.spendRecords();
        for (std::size_t i = 0; i < dRecords.size(); ++i) {
            for (std::size_t j = i + 1; j < dRecords.size(); ++j) {
                if (dRecords[i].spendId() == dRecords[j].spendId()) {
                    return false;
                }
            }
        }
    }

    return true;
}

} // namespace nodo::node
