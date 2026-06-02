#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_HPP

#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * FinalizedTreasurySection carries treasury execution evidence for one
 * finalized block. An empty section (no spends) is always valid.
 *
 * Security principle:
 * Evidence is canonical. Spend records are derived from evidence and kept
 * for report convenience. A non-empty section with spend records but no
 * execution evidence is rejected by FinalizedTreasurySectionValidator.
 *
 * Canonical construction:
 *   - use the evidence constructor when execution evidence is available;
 *   - the default constructor produces a valid empty section;
 *   - the legacy spendRecords constructor is retained for codec compatibility
 *     but produces a section that fails production validation when non-empty.
 *
 * Architecture note:
 * The evidence constructor derives spendRecords from the evidence so that
 * EpochTreasuryReport and ChainAuditor can sum spend totals without
 * duplicating evidence traversal.
 */
class FinalizedTreasurySection {
public:
    FinalizedTreasurySection();

    // Canonical constructor: build section from execution evidence.
    // SpendRecords are derived automatically from evidence.spendRecord().
    explicit FinalizedTreasurySection(
        std::vector<economics::TreasuryExecutionEvidence> executionEvidence
    );

    // Legacy constructor retained for codec compatibility.
    // Non-empty sections built this way will fail production validation
    // because they carry no execution evidence.
    explicit FinalizedTreasurySection(
        std::vector<economics::TreasurySpendRecord> spendRecords
    );

    const std::vector<economics::TreasuryExecutionEvidence>& executionEvidence() const;
    const std::vector<economics::TreasurySpendRecord>& spendRecords() const;

    std::size_t evidenceCount() const;
    std::size_t spendRecordCount() const;
    utils::Amount treasurySpendTotal() const;

    bool hasEvidence() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::vector<economics::TreasuryExecutionEvidence> m_executionEvidence;
    std::vector<economics::TreasurySpendRecord> m_spendRecords;
    utils::Amount m_treasurySpendTotal;
    bool m_hasEvidence;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::node

#endif
