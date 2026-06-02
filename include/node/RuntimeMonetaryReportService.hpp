#ifndef NODO_NODE_RUNTIME_MONETARY_REPORT_SERVICE_HPP
#define NODO_NODE_RUNTIME_MONETARY_REPORT_SERVICE_HPP

#include "economics/EpochMonetaryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::node {

enum class MonetaryReportServiceStatus {
    PERSISTED,
    EMPTY_DELTAS,
    REPORT_INVALID,
    PERSIST_FAILED,
    READBACK_FAILED,
    READBACK_MISMATCH
};

std::string monetaryReportServiceStatusToString(
    MonetaryReportServiceStatus status
);

class MonetaryReportServiceResult {
public:
    MonetaryReportServiceResult();

    static MonetaryReportServiceResult persisted(
        const economics::EpochMonetaryReport& report
    );

    static MonetaryReportServiceResult emptyDeltas();

    static MonetaryReportServiceResult reportInvalid(std::string reason);
    static MonetaryReportServiceResult persistFailed(std::string reason);
    static MonetaryReportServiceResult readbackFailed(std::string reason);
    static MonetaryReportServiceResult readbackMismatch(std::string reason);

    bool succeeded() const;
    MonetaryReportServiceStatus status() const;
    const std::string& reason() const;
    const economics::EpochMonetaryReport& report() const;

    std::string serialize() const;

private:
    MonetaryReportServiceStatus m_status;
    std::string m_reason;
    economics::EpochMonetaryReport m_report;
};

/*
 * RuntimeMonetaryReportService builds the current epoch monetary report from
 * finalized SupplyDeltas, persists it to disk, and verifies the read-back
 * matches the written data.
 *
 * Security principle:
 * The service does not persist a report that fails generation or read-back
 * verification. Only a fully validated report reaches disk.
 */
class RuntimeMonetaryReportService {
public:
    static MonetaryReportServiceResult buildAndPersist(
        const economics::MonetaryPolicy& policy,
        const std::vector<economics::SupplyDelta>& finalizedDeltas,
        std::uint64_t epoch,
        const std::filesystem::path& reportPath
    );
};

} // namespace nodo::node

#endif
