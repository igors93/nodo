#include "node/MonetaryAuditDiagnostic.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string monetaryAuditDiagnosticStatusToString(
    MonetaryAuditDiagnosticStatus status
) {
    switch (status) {
        case MonetaryAuditDiagnosticStatus::OK:
            return "OK";
        case MonetaryAuditDiagnosticStatus::SUPPLY_CONTINUITY_FAILURE:
            return "SUPPLY_CONTINUITY_FAILURE";
        case MonetaryAuditDiagnosticStatus::REPORT_MISMATCH:
            return "REPORT_MISMATCH";
        case MonetaryAuditDiagnosticStatus::REPORT_MISSING:
            return "REPORT_MISSING";
        default:
            return "UNKNOWN";
    }
}

MonetaryAuditDiagnostic::MonetaryAuditDiagnostic()
    : m_status(MonetaryAuditDiagnosticStatus::OK),
      m_reason(""),
      m_failedBlockHeight(0),
      m_expectedSupply(utils::Amount::fromRawUnits(0)),
      m_actualSupply(utils::Amount::fromRawUnits(0)),
      m_latestValidSupply(utils::Amount::fromRawUnits(0)),
      m_reportEpoch(0),
      m_reportExpectedEndingSupply(utils::Amount::fromRawUnits(0)),
      m_reportActualEndingSupply(utils::Amount::fromRawUnits(0)) {}

MonetaryAuditDiagnostic MonetaryAuditDiagnostic::ok() {
    MonetaryAuditDiagnostic d;
    d.m_status = MonetaryAuditDiagnosticStatus::OK;
    return d;
}

MonetaryAuditDiagnostic MonetaryAuditDiagnostic::supplyContinuityFailure(
    std::string reason,
    std::uint64_t failedBlockHeight,
    utils::Amount expectedSupply,
    utils::Amount actualSupply,
    utils::Amount latestValidSupply
) {
    MonetaryAuditDiagnostic d;
    d.m_status = MonetaryAuditDiagnosticStatus::SUPPLY_CONTINUITY_FAILURE;
    d.m_reason = std::move(reason);
    d.m_failedBlockHeight = failedBlockHeight;
    d.m_expectedSupply = expectedSupply;
    d.m_actualSupply = actualSupply;
    d.m_latestValidSupply = latestValidSupply;
    return d;
}

MonetaryAuditDiagnostic MonetaryAuditDiagnostic::reportMismatch(
    std::string reason,
    std::uint64_t reportEpoch,
    utils::Amount reportExpectedEndingSupply,
    utils::Amount reportActualEndingSupply
) {
    MonetaryAuditDiagnostic d;
    d.m_status = MonetaryAuditDiagnosticStatus::REPORT_MISMATCH;
    d.m_reason = std::move(reason);
    d.m_reportEpoch = reportEpoch;
    d.m_reportExpectedEndingSupply = reportExpectedEndingSupply;
    d.m_reportActualEndingSupply = reportActualEndingSupply;
    return d;
}

MonetaryAuditDiagnostic MonetaryAuditDiagnostic::reportMissing(
    std::string reason,
    std::uint64_t reportEpoch
) {
    MonetaryAuditDiagnostic d;
    d.m_status = MonetaryAuditDiagnosticStatus::REPORT_MISSING;
    d.m_reason = std::move(reason);
    d.m_reportEpoch = reportEpoch;
    return d;
}

MonetaryAuditDiagnosticStatus MonetaryAuditDiagnostic::status() const {
    return m_status;
}

bool MonetaryAuditDiagnostic::isOk() const {
    return m_status == MonetaryAuditDiagnosticStatus::OK;
}

const std::string& MonetaryAuditDiagnostic::reason() const { return m_reason; }
std::uint64_t MonetaryAuditDiagnostic::failedBlockHeight() const { return m_failedBlockHeight; }
utils::Amount MonetaryAuditDiagnostic::expectedSupply() const { return m_expectedSupply; }
utils::Amount MonetaryAuditDiagnostic::actualSupply() const { return m_actualSupply; }
utils::Amount MonetaryAuditDiagnostic::latestValidSupply() const { return m_latestValidSupply; }
std::uint64_t MonetaryAuditDiagnostic::reportEpoch() const { return m_reportEpoch; }
utils::Amount MonetaryAuditDiagnostic::reportExpectedEndingSupply() const { return m_reportExpectedEndingSupply; }
utils::Amount MonetaryAuditDiagnostic::reportActualEndingSupply() const { return m_reportActualEndingSupply; }

std::string MonetaryAuditDiagnostic::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryAuditDiagnostic{"
        << "status=" << monetaryAuditDiagnosticStatusToString(m_status)
        << ";reason=" << m_reason
        << ";failedBlockHeight=" << m_failedBlockHeight
        << ";expectedSupplyRaw=" << m_expectedSupply.rawUnits()
        << ";actualSupplyRaw=" << m_actualSupply.rawUnits()
        << ";latestValidSupplyRaw=" << m_latestValidSupply.rawUnits()
        << ";reportEpoch=" << m_reportEpoch
        << ";reportExpectedEndingSupplyRaw=" << m_reportExpectedEndingSupply.rawUnits()
        << ";reportActualEndingSupplyRaw=" << m_reportActualEndingSupply.rawUnits()
        << "}";
    return oss.str();
}

} // namespace nodo::node
