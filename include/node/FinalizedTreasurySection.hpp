#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_HPP

#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * FinalizedTreasurySection carries all TreasurySpendRecord entries for one
 * finalized block. An empty section (no spends) is always valid.
 *
 * Security principle:
 * Every spend record in the section must be individually valid. A single
 * invalid spend record invalidates the entire section. The section is
 * deterministic — the same spend records always produce the same total.
 */
class FinalizedTreasurySection {
public:
    FinalizedTreasurySection();

    explicit FinalizedTreasurySection(
        std::vector<economics::TreasurySpendRecord> spendRecords
    );

    const std::vector<economics::TreasurySpendRecord>& spendRecords() const;
    std::size_t spendRecordCount() const;
    utils::Amount treasurySpendTotal() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::vector<economics::TreasurySpendRecord> m_spendRecords;
    utils::Amount m_treasurySpendTotal;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::node

#endif
