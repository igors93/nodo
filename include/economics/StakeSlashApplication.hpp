#ifndef NODO_ECONOMICS_STAKE_SLASH_APPLICATION_HPP
#define NODO_ECONOMICS_STAKE_SLASH_APPLICATION_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "economics/ValidatorStakeState.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::economics {

enum class SlashResult {
    APPLIED,
    ALREADY_APPLIED,
    INVALID_EVIDENCE,
    WOULD_EXCEED_STAKE,
    VALIDATOR_TOMBSTONED
};

std::string slashResultToString(SlashResult result);

/*
 * StakeSlashApplication applies a penalty decision to a validator's stake.
 *
 * Security principle:
 * Idempotency is mandatory: the same evidence ID must not reduce stake twice.
 * The caller provides the evidence ID and slash amount; this class checks
 * idempotency, validates the amounts, and then delegates the actual deduction
 * to StakeAccount. Tombstoned validators cannot be slashed further.
 */
class StakeSlashApplication {
public:
    static SlashResult apply(
        ValidatorStakeState& state,
        const std::string& evidenceId,
        utils::Amount slashAmount
    );

    static SlashResult applyPenaltyDecision(
        ValidatorStakeState& state,
        const consensus::ValidatorPenaltyDecision& decision
    );
};

} // namespace nodo::economics

#endif
