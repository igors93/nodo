#include "node/LegacyTreasuryValidator.hpp"

#include <utility>

namespace nodo::node {

std::string legacyTreasuryValidationStatusToString(
    LegacyTreasuryValidationStatus status
) {
    switch (status) {
        case LegacyTreasuryValidationStatus::PASS:         return "PASS";
        case LegacyTreasuryValidationStatus::FAIL:         return "FAIL";
        case LegacyTreasuryValidationStatus::NOT_EVALUATED: return "NOT_EVALUATED";
        default:                                             return "UNKNOWN";
    }
}

LegacyTreasuryValidationResult::LegacyTreasuryValidationResult()
    : m_status(LegacyTreasuryValidationStatus::NOT_EVALUATED),
      m_reason("Uninitialized.") {}

LegacyTreasuryValidationResult LegacyTreasuryValidationResult::pass() {
    LegacyTreasuryValidationResult r;
    r.m_status = LegacyTreasuryValidationStatus::PASS;
    r.m_reason = "";
    return r;
}

LegacyTreasuryValidationResult LegacyTreasuryValidationResult::fail(std::string reason) {
    LegacyTreasuryValidationResult r;
    r.m_status = LegacyTreasuryValidationStatus::FAIL;
    r.m_reason = std::move(reason);
    return r;
}

LegacyTreasuryValidationResult LegacyTreasuryValidationResult::notEvaluated(
    std::string reason
) {
    LegacyTreasuryValidationResult r;
    r.m_status = LegacyTreasuryValidationStatus::NOT_EVALUATED;
    r.m_reason = std::move(reason);
    return r;
}

bool LegacyTreasuryValidationResult::passed() const {
    return m_status == LegacyTreasuryValidationStatus::PASS;
}

LegacyTreasuryValidationStatus LegacyTreasuryValidationResult::status() const {
    return m_status;
}

const std::string& LegacyTreasuryValidationResult::reason() const {
    return m_reason;
}

LegacyTreasuryValidationResult LegacyTreasuryValidator::validateGenesisConsistency(
    const GenesisTreasurySnapshot& snapshot,
    const economics::TreasuryAccount& treasury
) {
    if (!snapshot.active()) {
        return LegacyTreasuryValidationResult::notEvaluated(
            "GenesisTreasurySnapshot is not active — no cross-check required"
        );
    }

    if (!treasury.isValid()) {
        return LegacyTreasuryValidationResult::fail(
            "TreasuryAccount is invalid: " + treasury.rejectionReason()
        );
    }

    // The canonical treasury balance at genesis must equal the snapshot balance.
    // GenesisTreasurySnapshot.genesisTreasuryBalance() represents the total
    // protocol treasury at the start of the epoch, which must match the
    // TreasuryAccount balance that was funded from the same genesis allocation.
    if (treasury.balance() != snapshot.genesisTreasuryBalance()) {
        return LegacyTreasuryValidationResult::fail(
            "genesis treasury balance mismatch: "
            "TreasuryAccount.balance=" +
            std::to_string(treasury.balance().rawUnits()) +
            " GenesisTreasurySnapshot.genesisTreasuryBalance=" +
            std::to_string(snapshot.genesisTreasuryBalance().rawUnits())
        );
    }

    return LegacyTreasuryValidationResult::pass();
}

LegacyTreasuryValidationResult LegacyTreasuryValidator::validateTreasuryFeeRecord(
    const TreasuryFeeRecord& feeRecord,
    utils::Amount expectedTreasuryAmount
) {
    if (!feeRecord.active()) {
        return LegacyTreasuryValidationResult::notEvaluated(
            "TreasuryFeeRecord is not active — no cross-check required"
        );
    }

    if (feeRecord.treasuryAmount() != expectedTreasuryAmount) {
        return LegacyTreasuryValidationResult::fail(
            "TreasuryFeeRecord treasury amount mismatch: "
            "recorded=" + std::to_string(feeRecord.treasuryAmount().rawUnits()) +
            " expected=" + std::to_string(expectedTreasuryAmount.rawUnits())
        );
    }

    return LegacyTreasuryValidationResult::pass();
}

} // namespace nodo::node
