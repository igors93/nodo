#ifndef NODO_NODE_DEFENSE_MODE_TRANSITION_APPLIER_HPP
#define NODO_NODE_DEFENSE_MODE_TRANSITION_APPLIER_HPP

#include "economics/DefenseModeState.hpp"
#include "economics/DefenseModeTransitionRecord.hpp"
#include "node/RuntimeSafetyState.hpp"
#include "node/RuntimeSafetyStateStore.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

enum class DefenseModeTransitionApplyStatus {
    APPLIED,
    INVALID_TRANSITION,
    VALIDATION_REJECTED,
    PERSIST_FAILED,
    NO_STATE_CHANGE
};

std::string defenseModeTransitionApplyStatusToString(
    DefenseModeTransitionApplyStatus status
);

class DefenseModeTransitionApplyResult {
public:
    DefenseModeTransitionApplyResult();

    static DefenseModeTransitionApplyResult applied(RuntimeSafetyState newState);
    static DefenseModeTransitionApplyResult rejected(
        DefenseModeTransitionApplyStatus status,
        std::string reason
    );

    bool isApplied() const;
    DefenseModeTransitionApplyStatus status() const;
    const std::string& reason() const;
    const RuntimeSafetyState& newState() const;

private:
    DefenseModeTransitionApplyStatus m_status;
    std::string m_reason;
    RuntimeSafetyState m_newState;
};

/*
 * DefenseModeTransitionApplier validates a transition record, derives the new
 * RuntimeSafetyState, and persists it atomically.
 *
 * Security principle:
 * A transition that cannot be validated is never applied. A failed atomic write
 * leaves the prior state intact. The caller receives a deterministic result that
 * describes exactly what happened — there is no silent partial update.
 */
class DefenseModeTransitionApplier {
public:
    // Apply a validated activation (INACTIVE -> ACTIVE) transition.
    // persistPath: where to write the updated safety state.
    // updatedAt: timestamp for the new state record.
    static DefenseModeTransitionApplyResult applyActivation(
        const economics::DefenseModeTransitionRecord& transition,
        const RuntimeSafetyState& currentState,
        const std::filesystem::path& persistPath,
        std::int64_t updatedAt
    );

    // Apply a validated exit (ACTIVE -> INACTIVE) transition.
    // auditRequiredByPolicy: whether the current policy requires a chain audit.
    // minimumAuditHeight: the minimum audit height that satisfies the policy.
    static DefenseModeTransitionApplyResult applyExit(
        const economics::DefenseModeTransitionRecord& transition,
        const RuntimeSafetyState& currentState,
        bool auditRequiredByPolicy,
        std::uint64_t minimumAuditHeight,
        const std::filesystem::path& persistPath,
        std::int64_t updatedAt
    );
};

} // namespace nodo::node

#endif
