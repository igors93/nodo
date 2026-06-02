#include "node/RuntimeMonetaryReportService.hpp"

#include "node/EpochMonetaryReportStore.hpp"
#include "node/MonetaryReportVerifier.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

std::string monetaryReportServiceStatusToString(
    MonetaryReportServiceStatus status
) {
    switch (status) {
        case MonetaryReportServiceStatus::PERSISTED:          return "PERSISTED";
        case MonetaryReportServiceStatus::EMPTY_DELTAS:       return "EMPTY_DELTAS";
        case MonetaryReportServiceStatus::REPORT_INVALID:     return "REPORT_INVALID";
        case MonetaryReportServiceStatus::PERSIST_FAILED:     return "PERSIST_FAILED";
        case MonetaryReportServiceStatus::READBACK_FAILED:    return "READBACK_FAILED";
        case MonetaryReportServiceStatus::READBACK_MISMATCH:  return "READBACK_MISMATCH";
        default:                                               return "UNKNOWN";
    }
}

MonetaryReportServiceResult::MonetaryReportServiceResult()
    : m_status(MonetaryReportServiceStatus::REPORT_INVALID),
      m_reason("Uninitialized."),
      m_report() {}

MonetaryReportServiceResult MonetaryReportServiceResult::persisted(
    const economics::EpochMonetaryReport& report
) {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::PERSISTED;
    r.m_reason = "";
    r.m_report = report;
    return r;
}

MonetaryReportServiceResult MonetaryReportServiceResult::emptyDeltas() {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::EMPTY_DELTAS;
    r.m_reason = "no finalized deltas to report";
    return r;
}

MonetaryReportServiceResult MonetaryReportServiceResult::reportInvalid(
    std::string reason
) {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::REPORT_INVALID;
    r.m_reason = std::move(reason);
    return r;
}

MonetaryReportServiceResult MonetaryReportServiceResult::persistFailed(
    std::string reason
) {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::PERSIST_FAILED;
    r.m_reason = std::move(reason);
    return r;
}

MonetaryReportServiceResult MonetaryReportServiceResult::readbackFailed(
    std::string reason
) {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::READBACK_FAILED;
    r.m_reason = std::move(reason);
    return r;
}

MonetaryReportServiceResult MonetaryReportServiceResult::readbackMismatch(
    std::string reason
) {
    MonetaryReportServiceResult r;
    r.m_status = MonetaryReportServiceStatus::READBACK_MISMATCH;
    r.m_reason = std::move(reason);
    return r;
}

bool MonetaryReportServiceResult::succeeded() const {
    return m_status == MonetaryReportServiceStatus::PERSISTED;
}

MonetaryReportServiceStatus MonetaryReportServiceResult::status() const {
    return m_status;
}

const std::string& MonetaryReportServiceResult::reason() const {
    return m_reason;
}

const economics::EpochMonetaryReport& MonetaryReportServiceResult::report() const {
    return m_report;
}

std::string MonetaryReportServiceResult::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryReportServiceResult{"
        << "status=" << monetaryReportServiceStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

MonetaryReportServiceResult RuntimeMonetaryReportService::buildAndPersist(
    const economics::MonetaryPolicy& policy,
    const std::vector<economics::SupplyDelta>& finalizedDeltas,
    std::uint64_t epoch,
    const std::filesystem::path& reportPath
) {
    if (finalizedDeltas.empty()) {
        return MonetaryReportServiceResult::emptyDeltas();
    }

    const economics::EpochMonetaryReport report =
        economics::EpochMonetaryReport::fromDeltas(
            policy,
            epoch,
            finalizedDeltas.front().blockHeight(),
            finalizedDeltas.back().blockHeight(),
            finalizedDeltas
        );

    if (!report.isValid()) {
        return MonetaryReportServiceResult::reportInvalid(
            "report generation failed: " + report.rejectionReason()
        );
    }

    try {
        EpochMonetaryReportStore::write(reportPath, report);
    } catch (const std::exception& e) {
        return MonetaryReportServiceResult::persistFailed(
            std::string("failed to write report: ") + e.what()
        );
    }

    economics::EpochMonetaryReport readback;
    try {
        readback = EpochMonetaryReportStore::read(reportPath, policy);
    } catch (const std::exception& e) {
        return MonetaryReportServiceResult::readbackFailed(
            std::string("failed to read back report: ") + e.what()
        );
    }

    const MonetaryReportVerificationResult verif =
        MonetaryReportVerifier::verify(readback, report);

    if (!verif.matched()) {
        return MonetaryReportServiceResult::readbackMismatch(
            "read-back report does not match written report: " + verif.reason()
        );
    }

    return MonetaryReportServiceResult::persisted(report);
}

} // namespace nodo::node
