#ifndef NODO_NODE_MONETARY_REPORT_VERIFIER_HPP
#define NODO_NODE_MONETARY_REPORT_VERIFIER_HPP

#include "economics/EpochMonetaryReport.hpp"
#include "node/MonetaryAuditDiagnostic.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

enum class MonetaryReportVerificationStatus {
    MATCH,
    FIELD_MISMATCH,
    PERSISTED_INVALID,
    REBUILT_INVALID
};

std::string monetaryReportVerificationStatusToString(
    MonetaryReportVerificationStatus status
);

class MonetaryReportVerificationResult {
public:
    MonetaryReportVerificationResult();

    static MonetaryReportVerificationResult match();

    static MonetaryReportVerificationResult fieldMismatch(
        std::string reason,
        std::uint64_t epoch,
        utils::Amount expectedEndingSupply,
        utils::Amount actualEndingSupply
    );

    static MonetaryReportVerificationResult persistedInvalid(std::string reason);
    static MonetaryReportVerificationResult rebuiltInvalid(std::string reason);

    bool matched() const;
    MonetaryReportVerificationStatus status() const;
    const std::string& reason() const;
    const MonetaryAuditDiagnostic& diagnostic() const;

    std::string serialize() const;

private:
    MonetaryReportVerificationStatus m_status;
    std::string m_reason;
    MonetaryAuditDiagnostic m_diagnostic;
};

/*
 * MonetaryReportVerifier compares a persisted EpochMonetaryReport against a
 * report rebuilt deterministically from finalized SupplyDeltas.
 *
 * Security principle:
 * A persisted report is not authoritative. It must agree with the rebuilt
 * report on every field. Any discrepancy fails the audit.
 */
class MonetaryReportVerifier {
public:
    static MonetaryReportVerificationResult verify(
        const economics::EpochMonetaryReport& persisted,
        const economics::EpochMonetaryReport& rebuilt
    );
};

} // namespace nodo::node

#endif
