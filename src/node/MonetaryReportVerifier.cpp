#include "node/MonetaryReportVerifier.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string monetaryReportVerificationStatusToString(
    MonetaryReportVerificationStatus status
) {
    switch (status) {
        case MonetaryReportVerificationStatus::MATCH:
            return "MATCH";
        case MonetaryReportVerificationStatus::FIELD_MISMATCH:
            return "FIELD_MISMATCH";
        case MonetaryReportVerificationStatus::PERSISTED_INVALID:
            return "PERSISTED_INVALID";
        case MonetaryReportVerificationStatus::REBUILT_INVALID:
            return "REBUILT_INVALID";
        default:
            return "UNKNOWN";
    }
}

MonetaryReportVerificationResult::MonetaryReportVerificationResult()
    : m_status(MonetaryReportVerificationStatus::REBUILT_INVALID),
      m_reason("Uninitialized verification result."),
      m_diagnostic(MonetaryAuditDiagnostic::ok()) {}

MonetaryReportVerificationResult MonetaryReportVerificationResult::match() {
    MonetaryReportVerificationResult r;
    r.m_status = MonetaryReportVerificationStatus::MATCH;
    r.m_reason = "";
    r.m_diagnostic = MonetaryAuditDiagnostic::ok();
    return r;
}

MonetaryReportVerificationResult MonetaryReportVerificationResult::fieldMismatch(
    std::string reason,
    std::uint64_t epoch,
    utils::Amount expectedEndingSupply,
    utils::Amount actualEndingSupply
) {
    MonetaryReportVerificationResult r;
    r.m_status = MonetaryReportVerificationStatus::FIELD_MISMATCH;
    r.m_reason = reason;
    r.m_diagnostic = MonetaryAuditDiagnostic::reportMismatch(
        reason, epoch, expectedEndingSupply, actualEndingSupply
    );
    return r;
}

MonetaryReportVerificationResult MonetaryReportVerificationResult::persistedInvalid(
    std::string reason
) {
    MonetaryReportVerificationResult r;
    r.m_status = MonetaryReportVerificationStatus::PERSISTED_INVALID;
    r.m_reason = std::move(reason);
    r.m_diagnostic = MonetaryAuditDiagnostic::ok();
    return r;
}

MonetaryReportVerificationResult MonetaryReportVerificationResult::rebuiltInvalid(
    std::string reason
) {
    MonetaryReportVerificationResult r;
    r.m_status = MonetaryReportVerificationStatus::REBUILT_INVALID;
    r.m_reason = std::move(reason);
    r.m_diagnostic = MonetaryAuditDiagnostic::ok();
    return r;
}

bool MonetaryReportVerificationResult::matched() const {
    return m_status == MonetaryReportVerificationStatus::MATCH;
}

MonetaryReportVerificationStatus MonetaryReportVerificationResult::status() const {
    return m_status;
}

const std::string& MonetaryReportVerificationResult::reason() const {
    return m_reason;
}

const MonetaryAuditDiagnostic& MonetaryReportVerificationResult::diagnostic() const {
    return m_diagnostic;
}

std::string MonetaryReportVerificationResult::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryReportVerificationResult{"
        << "status=" << monetaryReportVerificationStatusToString(m_status)
        << ";reason=" << m_reason
        << ";diagnostic=" << m_diagnostic.serialize()
        << "}";
    return oss.str();
}

MonetaryReportVerificationResult MonetaryReportVerifier::verify(
    const economics::EpochMonetaryReport& persisted,
    const economics::EpochMonetaryReport& rebuilt
) {
    if (!persisted.isValid()) {
        return MonetaryReportVerificationResult::persistedInvalid(
            "persisted report is invalid: " + persisted.rejectionReason()
        );
    }

    if (!rebuilt.isValid()) {
        return MonetaryReportVerificationResult::rebuiltInvalid(
            "rebuilt report is invalid: " + rebuilt.rejectionReason()
        );
    }

    const std::uint64_t epoch = rebuilt.epoch();

    auto mismatch = [&](const std::string& field) {
        return MonetaryReportVerificationResult::fieldMismatch(
            "monetary report field mismatch: " + field,
            epoch,
            rebuilt.endingSupply(),
            persisted.endingSupply()
        );
    };

    if (persisted.epoch()        != rebuilt.epoch())        return mismatch("epoch");
    if (persisted.startBlock()   != rebuilt.startBlock())   return mismatch("startBlock");
    if (persisted.endBlock()     != rebuilt.endBlock())     return mismatch("endBlock");
    if (persisted.startingSupply() != rebuilt.startingSupply()) return mismatch("startingSupply");
    if (persisted.endingSupply() != rebuilt.endingSupply()) return mismatch("endingSupply");
    if (persisted.totalMinted()  != rebuilt.totalMinted())  return mismatch("totalMinted");
    if (persisted.totalBurned()  != rebuilt.totalBurned())  return mismatch("totalBurned");
    if (persisted.deltaCount()   != rebuilt.deltaCount())   return mismatch("deltaCount");
    if (persisted.mintRecordCount() != rebuilt.mintRecordCount()) return mismatch("mintRecordCount");
    if (persisted.burnRecordCount() != rebuilt.burnRecordCount()) return mismatch("burnRecordCount");
    if (persisted.policyVersion() != rebuilt.policyVersion()) return mismatch("policyVersion");

    return MonetaryReportVerificationResult::match();
}

} // namespace nodo::node
