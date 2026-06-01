#include "economics/StakeSlashApplication.hpp"

namespace nodo::economics {

std::string slashResultToString(SlashResult result) {
    switch (result) {
        case SlashResult::APPLIED:              return "APPLIED";
        case SlashResult::ALREADY_APPLIED:      return "ALREADY_APPLIED";
        case SlashResult::INVALID_EVIDENCE:     return "INVALID_EVIDENCE";
        case SlashResult::WOULD_EXCEED_STAKE:   return "WOULD_EXCEED_STAKE";
        case SlashResult::VALIDATOR_TOMBSTONED:  return "VALIDATOR_TOMBSTONED";
        default:                                return "UNKNOWN";
    }
}

SlashResult StakeSlashApplication::apply(
    ValidatorStakeState& state,
    const std::string& evidenceId,
    utils::Amount slashAmount
) {
    if (evidenceId.empty()) {
        return SlashResult::INVALID_EVIDENCE;
    }
    if (slashAmount.isNegative() || slashAmount.isZero()) {
        return SlashResult::INVALID_EVIDENCE;
    }

    if (state.hasAppliedEvidence(evidenceId)) {
        return SlashResult::ALREADY_APPLIED;
    }

    if (state.account().tombstoned()) {
        return SlashResult::VALIDATOR_TOMBSTONED;
    }

    if (!state.account().canSlash(slashAmount)) {
        return SlashResult::WOULD_EXCEED_STAKE;
    }

    state.recordAppliedEvidence(evidenceId);
    state.account().applySlash(slashAmount);
    state.updateBondingStatus();

    return SlashResult::APPLIED;
}

SlashResult StakeSlashApplication::applyPenaltyDecision(
    ValidatorStakeState& state,
    const consensus::ValidatorPenaltyDecision& decision
) {
    if (!decision.isValid() ||
        decision.evidenceId().empty() ||
        decision.validatorAddress() != state.account().validatorAddress()) {
        return SlashResult::INVALID_EVIDENCE;
    }

    if (state.hasAppliedEvidence(decision.evidenceId())) {
        return SlashResult::ALREADY_APPLIED;
    }

    if (state.account().tombstoned()) {
        return SlashResult::VALIDATOR_TOMBSTONED;
    }

    if (decision.slashable()) {
        const SlashResult slashResult =
            apply(
                state,
                decision.evidenceId(),
                utils::Amount::fromRawUnits(decision.slashAmountRawUnits())
            );

        if (slashResult != SlashResult::APPLIED &&
            slashResult != SlashResult::ALREADY_APPLIED) {
            return slashResult;
        }
    } else {
        state.recordAppliedEvidence(decision.evidenceId());
    }

    if (decision.tombstonesValidator()) {
        state.account().tombstone();
    } else if (decision.jailsValidator()) {
        state.account().jail();
    }

    state.updateBondingStatus();

    return SlashResult::APPLIED;
}

} // namespace nodo::economics
