#include "node/EpochTreasuryReportVerifier.hpp"

#include <utility>

namespace nodo::node {

std::string epochTreasuryVerificationStatusToString(
    EpochTreasuryVerificationStatus status
) {
    switch (status) {
        case EpochTreasuryVerificationStatus::MATCH:            return "MATCH";
        case EpochTreasuryVerificationStatus::FIELD_MISMATCH:   return "FIELD_MISMATCH";
        case EpochTreasuryVerificationStatus::PERSISTED_INVALID: return "PERSISTED_INVALID";
        case EpochTreasuryVerificationStatus::REBUILT_INVALID:  return "REBUILT_INVALID";
        default:                                                  return "UNKNOWN";
    }
}

EpochTreasuryVerificationResult::EpochTreasuryVerificationResult()
    : m_status(EpochTreasuryVerificationStatus::REBUILT_INVALID),
      m_reason("Uninitialized.") {}

EpochTreasuryVerificationResult EpochTreasuryVerificationResult::match() {
    EpochTreasuryVerificationResult r;
    r.m_status = EpochTreasuryVerificationStatus::MATCH;
    r.m_reason = "";
    return r;
}

EpochTreasuryVerificationResult EpochTreasuryVerificationResult::fieldMismatch(
    std::string reason
) {
    EpochTreasuryVerificationResult r;
    r.m_status = EpochTreasuryVerificationStatus::FIELD_MISMATCH;
    r.m_reason = std::move(reason);
    return r;
}

EpochTreasuryVerificationResult EpochTreasuryVerificationResult::persistedInvalid(
    std::string reason
) {
    EpochTreasuryVerificationResult r;
    r.m_status = EpochTreasuryVerificationStatus::PERSISTED_INVALID;
    r.m_reason = std::move(reason);
    return r;
}

EpochTreasuryVerificationResult EpochTreasuryVerificationResult::rebuiltInvalid(
    std::string reason
) {
    EpochTreasuryVerificationResult r;
    r.m_status = EpochTreasuryVerificationStatus::REBUILT_INVALID;
    r.m_reason = std::move(reason);
    return r;
}

bool EpochTreasuryVerificationResult::matched() const {
    return m_status == EpochTreasuryVerificationStatus::MATCH;
}

EpochTreasuryVerificationStatus EpochTreasuryVerificationResult::status() const {
    return m_status;
}

const std::string& EpochTreasuryVerificationResult::reason() const {
    return m_reason;
}

EpochTreasuryVerificationResult EpochTreasuryReportVerifier::verify(
    const economics::EpochTreasuryReport& persisted,
    const economics::EpochTreasuryReport& rebuilt
) {
    if (!persisted.isValid()) {
        return EpochTreasuryVerificationResult::persistedInvalid(
            "persisted treasury report is invalid: " + persisted.rejectionReason()
        );
    }

    if (!rebuilt.isValid()) {
        return EpochTreasuryVerificationResult::rebuiltInvalid(
            "rebuilt treasury report is invalid: " + rebuilt.rejectionReason()
        );
    }

    if (persisted.epoch() != rebuilt.epoch()) {
        return EpochTreasuryVerificationResult::fieldMismatch(
            "treasury report epoch mismatch: persisted=" +
            std::to_string(persisted.epoch()) +
            " rebuilt=" + std::to_string(rebuilt.epoch())
        );
    }

    if (persisted.treasurySpendTotal() != rebuilt.treasurySpendTotal()) {
        return EpochTreasuryVerificationResult::fieldMismatch(
            "treasury report treasurySpendTotal mismatch: persisted=" +
            std::to_string(persisted.treasurySpendTotal().rawUnits()) +
            " rebuilt=" + std::to_string(rebuilt.treasurySpendTotal().rawUnits())
        );
    }

    if (persisted.spendRecordCount() != rebuilt.spendRecordCount()) {
        return EpochTreasuryVerificationResult::fieldMismatch(
            "treasury report spendRecordCount mismatch: persisted=" +
            std::to_string(persisted.spendRecordCount()) +
            " rebuilt=" + std::to_string(rebuilt.spendRecordCount())
        );
    }

    return EpochTreasuryVerificationResult::match();
}

} // namespace nodo::node
