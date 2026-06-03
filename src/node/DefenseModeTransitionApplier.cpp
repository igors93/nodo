#include "node/DefenseModeTransitionApplier.hpp"

#include <utility>

namespace nodo::node {

// ---- DefenseModeTransitionApplyResult ----

std::string defenseModeTransitionApplyStatusToString(
    DefenseModeTransitionApplyStatus status
) {
    switch (status) {
        case DefenseModeTransitionApplyStatus::APPLIED:
            return "APPLIED";
        case DefenseModeTransitionApplyStatus::INVALID_TRANSITION:
            return "INVALID_TRANSITION";
        case DefenseModeTransitionApplyStatus::VALIDATION_REJECTED:
            return "VALIDATION_REJECTED";
        case DefenseModeTransitionApplyStatus::PERSIST_FAILED:
            return "PERSIST_FAILED";
        case DefenseModeTransitionApplyStatus::NO_STATE_CHANGE:
            return "NO_STATE_CHANGE";
        default:
            return "UNKNOWN";
    }
}

DefenseModeTransitionApplyResult::DefenseModeTransitionApplyResult()
    : m_status(DefenseModeTransitionApplyStatus::INVALID_TRANSITION),
      m_reason(""),
      m_newState() {}

DefenseModeTransitionApplyResult DefenseModeTransitionApplyResult::applied(
    RuntimeSafetyState newState
) {
    DefenseModeTransitionApplyResult r;
    r.m_status = DefenseModeTransitionApplyStatus::APPLIED;
    r.m_newState = std::move(newState);
    return r;
}

DefenseModeTransitionApplyResult DefenseModeTransitionApplyResult::rejected(
    DefenseModeTransitionApplyStatus status,
    std::string reason
) {
    DefenseModeTransitionApplyResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool DefenseModeTransitionApplyResult::isApplied() const {
    return m_status == DefenseModeTransitionApplyStatus::APPLIED;
}

DefenseModeTransitionApplyStatus DefenseModeTransitionApplyResult::status() const {
    return m_status;
}

const std::string& DefenseModeTransitionApplyResult::reason() const {
    return m_reason;
}

const RuntimeSafetyState& DefenseModeTransitionApplyResult::newState() const {
    return m_newState;
}

// ---- DefenseModeTransitionApplier ----

DefenseModeTransitionApplyResult DefenseModeTransitionApplier::applyActivation(
    const economics::DefenseModeTransitionRecord& transition,
    const RuntimeSafetyState& currentState,
    const std::filesystem::path& persistPath,
    std::int64_t updatedAt
) {
    // Verify the current state is INACTIVE.
    if (currentState.isValid() &&
        currentState.defenseMode() == economics::DefenseModeState::ACTIVE) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::NO_STATE_CHANGE,
            "defense mode is already ACTIVE; cannot apply activation transition"
        );
    }

    // Validate the transition.
    const economics::DefenseModeTransitionResult validationResult =
        economics::DefenseModeTransitionValidator::validateActivation(transition);

    if (!validationResult.isAccepted()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::VALIDATION_REJECTED,
            "activation transition rejected: " + validationResult.reason()
        );
    }

    // Build new safety state.
    const RuntimeSafetyState newState(
        economics::DefenseModeState::ACTIVE,
        transition.trigger(),
        transition.blockHeight(),
        transition.reason(),
        transition.evidenceId(),
        transition.governanceProposalId(),
        currentState.isValid() ? currentState.lastChainAuditHeight() : 0,
        true, // exit requires chain audit by default
        updatedAt
    );

    if (!newState.isValid()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::INVALID_TRANSITION,
            "derived safety state is invalid: " + newState.rejectionReason()
        );
    }

    // Persist atomically.
    const RuntimeSafetyStateWriteResult writeResult =
        RuntimeSafetyStateStore::write(persistPath, newState);

    if (!writeResult.isWritten()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::PERSIST_FAILED,
            "failed to persist safety state: " + writeResult.reason()
        );
    }

    return DefenseModeTransitionApplyResult::applied(newState);
}

DefenseModeTransitionApplyResult DefenseModeTransitionApplier::applyExit(
    const economics::DefenseModeTransitionRecord& transition,
    const RuntimeSafetyState& currentState,
    bool auditRequiredByPolicy,
    std::uint64_t minimumAuditHeight,
    const std::filesystem::path& persistPath,
    std::int64_t updatedAt
) {
    // Verify the current state is ACTIVE.
    if (currentState.isValid() &&
        currentState.defenseMode() == economics::DefenseModeState::INACTIVE) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::NO_STATE_CHANGE,
            "defense mode is already INACTIVE; cannot apply exit transition"
        );
    }

    // Validate the exit transition.
    const economics::DefenseModeTransitionResult validationResult =
        economics::DefenseModeTransitionValidator::validateExit(
            transition, auditRequiredByPolicy, minimumAuditHeight
        );

    if (!validationResult.isAccepted()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::VALIDATION_REJECTED,
            "exit transition rejected: " + validationResult.reason()
        );
    }

    // Build new INACTIVE state.
    const RuntimeSafetyState newState(
        economics::DefenseModeState::INACTIVE,
        transition.trigger(),
        0,  // activationHeight zero when INACTIVE
        "", // activationReason empty when INACTIVE
        "", // evidenceId empty when INACTIVE
        "", // governanceProposalId empty when INACTIVE
        transition.chainAuditHeight(),
        currentState.isValid() ? currentState.exitRequiresChainAudit() : true,
        updatedAt
    );

    if (!newState.isValid()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::INVALID_TRANSITION,
            "derived safety state is invalid: " + newState.rejectionReason()
        );
    }

    // Persist atomically.
    const RuntimeSafetyStateWriteResult writeResult =
        RuntimeSafetyStateStore::write(persistPath, newState);

    if (!writeResult.isWritten()) {
        return DefenseModeTransitionApplyResult::rejected(
            DefenseModeTransitionApplyStatus::PERSIST_FAILED,
            "failed to persist safety state: " + writeResult.reason()
        );
    }

    return DefenseModeTransitionApplyResult::applied(newState);
}

} // namespace nodo::node
