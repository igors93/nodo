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

    // Digest comparison: when both reports carry a digest, compare them first.
    // This catches same-total/same-count mismatches where recipients, proposals,
    // or amounts differ. The rebuilt report always has a digest (from records);
    // the persisted report may be an older format with an empty digest.
    if (!persisted.spendRecordsDigest().empty() &&
        !rebuilt.spendRecordsDigest().empty() &&
        persisted.spendRecordsDigest() != rebuilt.spendRecordsDigest()) {
        return EpochTreasuryVerificationResult::fieldMismatch(
            "treasury report spendRecordsDigest mismatch: "
            "persisted=" + persisted.spendRecordsDigest() +
            " rebuilt=" + rebuilt.spendRecordsDigest()
        );
    }

    // Record-level comparison when both reports carry individual records.
    if (persisted.hasSpendRecords() && rebuilt.hasSpendRecords()) {
        const auto& pRecords = persisted.spendRecords();
        const auto& rRecords = rebuilt.spendRecords();

        // Detect duplicate spend identifiers in the rebuilt set.
        for (std::size_t i = 0; i < rRecords.size(); ++i) {
            for (std::size_t j = i + 1; j < rRecords.size(); ++j) {
                if (rRecords[i].spendId() == rRecords[j].spendId()) {
                    return EpochTreasuryVerificationResult::fieldMismatch(
                        "duplicate spend identifier in rebuilt report: " +
                        rRecords[i].spendId()
                    );
                }
            }
        }

        // Per-record identity comparison in canonical order.
        for (std::size_t i = 0; i < pRecords.size() && i < rRecords.size(); ++i) {
            const auto& p = pRecords[i];
            const auto& r = rRecords[i];

            if (p.spendId() != r.spendId()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " spendId mismatch: persisted=" + p.spendId() +
                    " rebuilt=" + r.spendId()
                );
            }
            if (p.proposalId() != r.proposalId()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " proposalId mismatch: persisted=" + p.proposalId() +
                    " rebuilt=" + r.proposalId()
                );
            }
            if (p.recipientAddress() != r.recipientAddress()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " recipientAddress mismatch: persisted=" + p.recipientAddress() +
                    " rebuilt=" + r.recipientAddress()
                );
            }
            if (p.amount() != r.amount()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " amount mismatch: persisted=" +
                    std::to_string(p.amount().rawUnits()) +
                    " rebuilt=" + std::to_string(r.amount().rawUnits())
                );
            }
            if (p.executedAtBlock() != r.executedAtBlock()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " executedAtBlock mismatch: persisted=" +
                    std::to_string(p.executedAtBlock()) +
                    " rebuilt=" + std::to_string(r.executedAtBlock())
                );
            }
            if (p.epoch() != r.epoch()) {
                return EpochTreasuryVerificationResult::fieldMismatch(
                    "spend record at index " + std::to_string(i) +
                    " epoch mismatch: persisted=" + std::to_string(p.epoch()) +
                    " rebuilt=" + std::to_string(r.epoch())
                );
            }
        }
    } else if (rebuilt.hasSpendRecords()) {
        // Only the rebuilt side has records. Detect duplicates in it.
        const auto& rRecords = rebuilt.spendRecords();
        for (std::size_t i = 0; i < rRecords.size(); ++i) {
            for (std::size_t j = i + 1; j < rRecords.size(); ++j) {
                if (rRecords[i].spendId() == rRecords[j].spendId()) {
                    return EpochTreasuryVerificationResult::fieldMismatch(
                        "duplicate spend identifier in rebuilt report: " +
                        rRecords[i].spendId()
                    );
                }
            }
        }
    }

    return EpochTreasuryVerificationResult::match();
}

} // namespace nodo::node
