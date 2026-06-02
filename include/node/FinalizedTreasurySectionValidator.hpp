#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_VALIDATOR_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_VALIDATOR_HPP

#include "node/FinalizedTreasurySection.hpp"

#include <string>

namespace nodo::node {

enum class TreasurySectionValidationStatus {
    VALID,
    INVALID_SECTION,
    INVALID_SPEND_RECORD,
    SPEND_WITHOUT_EVIDENCE,
    INVALID_EVIDENCE,
    EVIDENCE_SPEND_MISMATCH,
    MISSING_GOVERNANCE_CONTEXT,
    INVALID_GOVERNANCE_CONTEXT
};

std::string treasurySectionValidationStatusToString(
    TreasurySectionValidationStatus status
);

class TreasurySectionValidationResult {
public:
    TreasurySectionValidationResult();

    static TreasurySectionValidationResult valid();
    static TreasurySectionValidationResult invalidSection(std::string reason);
    static TreasurySectionValidationResult invalidSpendRecord(
        std::size_t index, std::string reason
    );
    static TreasurySectionValidationResult spendWithoutEvidence(std::string reason);
    static TreasurySectionValidationResult invalidEvidence(
        std::size_t index, std::string reason
    );
    static TreasurySectionValidationResult evidenceSpendMismatch(
        std::size_t index, std::string reason
    );
    static TreasurySectionValidationResult missingGovernanceContext(
        std::size_t index, std::string reason
    );
    static TreasurySectionValidationResult invalidGovernanceContext(
        std::size_t index, std::string reason
    );

    bool passed() const;
    TreasurySectionValidationStatus status() const;
    const std::string& reason() const;

private:
    TreasurySectionValidationStatus m_status;
    std::string m_reason;
};

/*
 * FinalizedTreasurySectionValidator validates all spend records and execution
 * evidence in a treasury section.
 *
 * Security principle:
 * A non-empty treasury section must carry execution evidence for every spend
 * record. A spend record without matching evidence is rejected in production
 * validation. An empty section (no spends, no evidence) is always valid.
 */
class FinalizedTreasurySectionValidator {
public:
    static TreasurySectionValidationResult validate(
        const FinalizedTreasurySection& section
    );
};

} // namespace nodo::node

#endif
