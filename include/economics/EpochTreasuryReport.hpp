#ifndef NODO_ECONOMICS_EPOCH_TREASURY_REPORT_HPP
#define NODO_ECONOMICS_EPOCH_TREASURY_REPORT_HPP

#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * EpochTreasuryReport summarizes treasury spend activity for one epoch.
 *
 * Security principle:
 * Treasury spend totals are derived from finalized TreasurySpendRecord entries.
 * A persisted EpochTreasuryReport must be verifiable against the canonical
 * spend record sequence — arbitrary totals are not accepted.
 */
class EpochTreasuryReport {
public:
    EpochTreasuryReport();

    // Build from canonical spend record sequence. The total is derived, not supplied.
    // The individual records are retained for record-level comparison.
    static EpochTreasuryReport fromSpendRecords(
        std::uint64_t epoch,
        const std::vector<TreasurySpendRecord>& spendRecords
    );

    // Reconstruct from stored fields (used on reload). The caller must verify
    // the reconstructed report against the canonical sequence.
    // spendRecordsDigest may be empty for reports written before the digest was introduced.
    static EpochTreasuryReport fromStoredFields(
        std::uint64_t epoch,
        utils::Amount treasurySpendTotal,
        std::size_t spendRecordCount,
        std::string spendRecordsDigest = ""
    );

    std::uint64_t epoch() const;
    utils::Amount treasurySpendTotal() const;
    std::size_t spendRecordCount() const;

    // Returns individual spend records when the report was built from records.
    // Empty when the report was rebuilt from stored totals only.
    const std::vector<TreasurySpendRecord>& spendRecords() const;
    // Returns true when this report was built from fromSpendRecords() and
    // therefore carries the full canonical record set. Returns false when
    // built from fromStoredFields() (loaded from persisted storage).
    bool hasSpendRecords() const;

    // Returns a deterministic SHA-256 digest of the ordered canonical spend records.
    // Empty string when the report was built from stored totals only.
    // Two reports with the same total/count but different recipients, proposals,
    // or amounts will produce different digests.
    const std::string& spendRecordsDigest() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::uint64_t m_epoch;
    utils::Amount m_treasurySpendTotal;
    std::size_t m_spendRecordCount;
    std::vector<TreasurySpendRecord> m_spendRecords;
    std::string m_spendRecordsDigest;
    bool m_hasSpendRecords;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
