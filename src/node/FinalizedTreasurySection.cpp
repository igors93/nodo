#include "node/FinalizedTreasurySection.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace nodo::node {

FinalizedTreasurySection::FinalizedTreasurySection()
    : m_treasurySpendTotal(utils::Amount::fromRawUnits(0)),
      m_valid(true),
      m_rejectionReason("") {}

FinalizedTreasurySection::FinalizedTreasurySection(
    std::vector<economics::TreasurySpendRecord> spendRecords
)
    : m_spendRecords(std::move(spendRecords)),
      m_treasurySpendTotal(utils::Amount::fromRawUnits(0)),
      m_valid(false),
      m_rejectionReason("")
{
    std::int64_t runningTotal = 0;

    for (std::size_t i = 0; i < m_spendRecords.size(); ++i) {
        const auto& rec = m_spendRecords[i];
        if (!rec.isValid()) {
            m_rejectionReason =
                "FinalizedTreasurySection: spend record at index " +
                std::to_string(i) + " is invalid: " + rec.rejectionReason();
            return;
        }
        // Overflow guard for running total.
        if (rec.amount().rawUnits() >
            std::numeric_limits<std::int64_t>::max() - runningTotal) {
            m_rejectionReason =
                "FinalizedTreasurySection: treasury spend total would overflow int64 "
                "at record index " + std::to_string(i);
            return;
        }
        runningTotal += rec.amount().rawUnits();
    }

    m_treasurySpendTotal = utils::Amount::fromRawUnits(runningTotal);
    m_valid = true;
}

const std::vector<economics::TreasurySpendRecord>&
FinalizedTreasurySection::spendRecords() const {
    return m_spendRecords;
}

std::size_t FinalizedTreasurySection::spendRecordCount() const {
    return m_spendRecords.size();
}

utils::Amount FinalizedTreasurySection::treasurySpendTotal() const {
    return m_treasurySpendTotal;
}

bool FinalizedTreasurySection::isValid() const { return m_valid; }

const std::string& FinalizedTreasurySection::rejectionReason() const {
    return m_rejectionReason;
}

std::string FinalizedTreasurySection::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedTreasurySection{"
        << "spendRecordCount=" << m_spendRecords.size()
        << ";treasurySpendTotalRaw=" << m_treasurySpendTotal.rawUnits()
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::node
