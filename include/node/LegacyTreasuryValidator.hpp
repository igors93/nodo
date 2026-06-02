#ifndef NODO_NODE_LEGACY_TREASURY_VALIDATOR_HPP
#define NODO_NODE_LEGACY_TREASURY_VALIDATOR_HPP

#include "economics/TreasuryAccount.hpp"
#include "node/FeeEconomics.hpp"
#include "node/ProtectionTreasury.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::node {

enum class LegacyTreasuryValidationStatus {
    PASS,
    FAIL,
    NOT_EVALUATED
};

std::string legacyTreasuryValidationStatusToString(
    LegacyTreasuryValidationStatus status
);

class LegacyTreasuryValidationResult {
public:
    LegacyTreasuryValidationResult();

    static LegacyTreasuryValidationResult pass();
    static LegacyTreasuryValidationResult fail(std::string reason);
    static LegacyTreasuryValidationResult notEvaluated(std::string reason);

    bool passed() const;
    LegacyTreasuryValidationStatus status() const;
    const std::string& reason() const;

private:
    LegacyTreasuryValidationStatus m_status;
    std::string m_reason;
};

/*
 * LegacyTreasuryValidator cross-validates legacy treasury types against the
 * canonical TreasuryAccount model introduced in Task 07.
 *
 * Security principle:
 * Legacy records (GenesisTreasurySnapshot, TreasuryFeeRecord) cannot
 * contradict the canonical treasury model. Any inconsistency must be detected
 * and rejected before the state is accepted.
 *
 * Full integration with the finalization pipeline is deferred to Task 09.
 * This validator provides the focused cross-check rules with tests.
 */
class LegacyTreasuryValidator {
public:
    // Verify that GenesisTreasurySnapshot and TreasuryAccount agree on the
    // initial treasury balance. They must both reflect the same genesis funds.
    static LegacyTreasuryValidationResult validateGenesisConsistency(
        const GenesisTreasurySnapshot& snapshot,
        const economics::TreasuryAccount& treasury
    );

    // Verify that TreasuryFeeRecord treasury amount matches the expected value.
    // The expected amount is derived from the canonical fee economics model.
    static LegacyTreasuryValidationResult validateTreasuryFeeRecord(
        const TreasuryFeeRecord& feeRecord,
        utils::Amount expectedTreasuryAmount
    );
};

} // namespace nodo::node

#endif
