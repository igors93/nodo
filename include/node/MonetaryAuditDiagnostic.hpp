#ifndef NODO_NODE_MONETARY_AUDIT_DIAGNOSTIC_HPP
#define NODO_NODE_MONETARY_AUDIT_DIAGNOSTIC_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

enum class MonetaryAuditDiagnosticStatus {
    OK,
    SUPPLY_CONTINUITY_FAILURE,
    REPORT_MISMATCH,
    REPORT_MISSING
};

std::string monetaryAuditDiagnosticStatusToString(
    MonetaryAuditDiagnosticStatus status
);

/*
 * MonetaryAuditDiagnostic captures operator-level detail for monetary failures.
 *
 * It is not user-facing. It is designed so an operator or developer can
 * identify the exact block height and supply values at which continuity or
 * report verification failed, without reading raw audit logs.
 */
class MonetaryAuditDiagnostic {
public:
    MonetaryAuditDiagnostic();

    static MonetaryAuditDiagnostic ok();

    static MonetaryAuditDiagnostic supplyContinuityFailure(
        std::string reason,
        std::uint64_t failedBlockHeight,
        utils::Amount expectedSupply,
        utils::Amount actualSupply,
        utils::Amount latestValidSupply
    );

    static MonetaryAuditDiagnostic reportMismatch(
        std::string reason,
        std::uint64_t reportEpoch,
        utils::Amount reportExpectedEndingSupply,
        utils::Amount reportActualEndingSupply
    );

    static MonetaryAuditDiagnostic reportMissing(
        std::string reason,
        std::uint64_t reportEpoch
    );

    MonetaryAuditDiagnosticStatus status() const;
    bool isOk() const;
    const std::string& reason() const;
    std::uint64_t failedBlockHeight() const;
    utils::Amount expectedSupply() const;
    utils::Amount actualSupply() const;
    utils::Amount latestValidSupply() const;
    std::uint64_t reportEpoch() const;
    utils::Amount reportExpectedEndingSupply() const;
    utils::Amount reportActualEndingSupply() const;

    std::string serialize() const;

private:
    MonetaryAuditDiagnosticStatus m_status;
    std::string m_reason;
    std::uint64_t m_failedBlockHeight;
    utils::Amount m_expectedSupply;
    utils::Amount m_actualSupply;
    utils::Amount m_latestValidSupply;
    std::uint64_t m_reportEpoch;
    utils::Amount m_reportExpectedEndingSupply;
    utils::Amount m_reportActualEndingSupply;
};

} // namespace nodo::node

#endif
