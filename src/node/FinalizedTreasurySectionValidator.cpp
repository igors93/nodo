#include "node/FinalizedTreasurySectionValidator.hpp"

#include <utility>

namespace nodo::node {

std::string treasurySectionValidationStatusToString(
    TreasurySectionValidationStatus status
) {
    switch (status) {
        case TreasurySectionValidationStatus::VALID:
            return "VALID";
        case TreasurySectionValidationStatus::INVALID_SECTION:
            return "INVALID_SECTION";
        case TreasurySectionValidationStatus::INVALID_SPEND_RECORD:
            return "INVALID_SPEND_RECORD";
        default:
            return "UNKNOWN";
    }
}

TreasurySectionValidationResult::TreasurySectionValidationResult()
    : m_status(TreasurySectionValidationStatus::INVALID_SECTION),
      m_reason("Uninitialized.") {}

TreasurySectionValidationResult TreasurySectionValidationResult::valid() {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::VALID;
    r.m_reason = "";
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidSection(
    std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_SECTION;
    r.m_reason = std::move(reason);
    return r;
}

TreasurySectionValidationResult TreasurySectionValidationResult::invalidSpendRecord(
    std::size_t index, std::string reason
) {
    TreasurySectionValidationResult r;
    r.m_status = TreasurySectionValidationStatus::INVALID_SPEND_RECORD;
    r.m_reason = "spend record at index " + std::to_string(index) +
                 " is invalid: " + std::move(reason);
    return r;
}

bool TreasurySectionValidationResult::passed() const {
    return m_status == TreasurySectionValidationStatus::VALID;
}

TreasurySectionValidationStatus TreasurySectionValidationResult::status() const {
    return m_status;
}

const std::string& TreasurySectionValidationResult::reason() const {
    return m_reason;
}

TreasurySectionValidationResult FinalizedTreasurySectionValidator::validate(
    const FinalizedTreasurySection& section
) {
    if (!section.isValid()) {
        return TreasurySectionValidationResult::invalidSection(
            section.rejectionReason()
        );
    }

    // Every spend record must be individually valid. FinalizedTreasurySection
    // already enforces this in its constructor, but we re-validate here to
    // ensure no invalid record can survive a round-trip through deserialization.
    for (std::size_t i = 0; i < section.spendRecords().size(); ++i) {
        const auto& rec = section.spendRecords()[i];
        if (!rec.isValid()) {
            return TreasurySectionValidationResult::invalidSpendRecord(
                i, rec.rejectionReason()
            );
        }
    }

    return TreasurySectionValidationResult::valid();
}

} // namespace nodo::node
