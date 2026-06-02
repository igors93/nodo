#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_VALIDATOR_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_VALIDATOR_HPP

#include "node/FinalizedTreasurySection.hpp"

#include <string>

namespace nodo::node {

enum class TreasurySectionValidationStatus {
    VALID,
    INVALID_SECTION,
    INVALID_SPEND_RECORD
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

    bool passed() const;
    TreasurySectionValidationStatus status() const;
    const std::string& reason() const;

private:
    TreasurySectionValidationStatus m_status;
    std::string m_reason;
};

/*
 * FinalizedTreasurySectionValidator validates all spend records in a section.
 *
 * Security principle:
 * No spend record may be included in a finalized artifact without individual
 * validation. A single invalid record makes the whole section unacceptable.
 */
class FinalizedTreasurySectionValidator {
public:
    static TreasurySectionValidationResult validate(
        const FinalizedTreasurySection& section
    );
};

} // namespace nodo::node

#endif
