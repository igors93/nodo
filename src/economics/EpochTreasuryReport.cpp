#include "economics/EpochTreasuryReport.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace nodo::economics {

EpochTreasuryReport::EpochTreasuryReport()
    : m_epoch(0),
      m_treasurySpendTotal(utils::Amount::fromRawUnits(0)),
      m_spendRecordCount(0),
      m_valid(false),
      m_rejectionReason("EpochTreasuryReport: default-constructed.") {}

EpochTreasuryReport EpochTreasuryReport::fromSpendRecords(
    std::uint64_t epoch,
    const std::vector<TreasurySpendRecord>& spendRecords
) {
    EpochTreasuryReport r;
    r.m_epoch = epoch;
    r.m_spendRecordCount = spendRecords.size();
    r.m_rejectionReason = "";

    std::int64_t runningTotal = 0;
    for (std::size_t i = 0; i < spendRecords.size(); ++i) {
        const auto& rec = spendRecords[i];
        if (!rec.isValid()) {
            r.m_rejectionReason =
                "EpochTreasuryReport: spend record at index " +
                std::to_string(i) + " is invalid: " + rec.rejectionReason();
            return r;
        }
        if (rec.amount().rawUnits() >
            std::numeric_limits<std::int64_t>::max() - runningTotal) {
            r.m_rejectionReason =
                "EpochTreasuryReport: treasury spend total would overflow int64 "
                "at record index " + std::to_string(i);
            return r;
        }
        runningTotal += rec.amount().rawUnits();
    }

    r.m_treasurySpendTotal = utils::Amount::fromRawUnits(runningTotal);
    r.m_valid = true;
    return r;
}

EpochTreasuryReport EpochTreasuryReport::fromStoredFields(
    std::uint64_t epoch,
    utils::Amount treasurySpendTotal,
    std::size_t spendRecordCount
) {
    EpochTreasuryReport r;
    r.m_epoch = epoch;
    r.m_treasurySpendTotal = treasurySpendTotal;
    r.m_spendRecordCount = spendRecordCount;
    r.m_rejectionReason = "";
    r.m_valid = true;
    return r;
}

std::uint64_t EpochTreasuryReport::epoch() const { return m_epoch; }
utils::Amount EpochTreasuryReport::treasurySpendTotal() const { return m_treasurySpendTotal; }
std::size_t EpochTreasuryReport::spendRecordCount() const { return m_spendRecordCount; }
bool EpochTreasuryReport::isValid() const { return m_valid; }
const std::string& EpochTreasuryReport::rejectionReason() const { return m_rejectionReason; }

std::string EpochTreasuryReport::serialize() const {
    std::ostringstream oss;
    oss << "EpochTreasuryReport{"
        << "epoch=" << m_epoch
        << ";treasurySpendTotalRaw=" << m_treasurySpendTotal.rawUnits()
        << ";spendRecordCount=" << m_spendRecordCount
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
