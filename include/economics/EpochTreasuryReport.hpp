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
    static EpochTreasuryReport fromSpendRecords(
        std::uint64_t epoch,
        const std::vector<TreasurySpendRecord>& spendRecords
    );

    // Reconstruct from stored fields (used on reload). The caller must verify
    // the reconstructed report against the canonical sequence.
    static EpochTreasuryReport fromStoredFields(
        std::uint64_t epoch,
        utils::Amount treasurySpendTotal,
        std::size_t spendRecordCount
    );

    std::uint64_t epoch() const;
    utils::Amount treasurySpendTotal() const;
    std::size_t spendRecordCount() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::uint64_t m_epoch;
    utils::Amount m_treasurySpendTotal;
    std::size_t m_spendRecordCount;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
