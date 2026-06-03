#ifndef NODO_NODE_DEFENSE_MODE_GUARD_HPP
#define NODO_NODE_DEFENSE_MODE_GUARD_HPP

#include "economics/DefenseModeState.hpp"

#include <string>

namespace nodo::node {

enum class DefenseGateStatus {
    ALLOWED,
    BLOCKED_DEFENSE_MODE_ACTIVE
};

std::string defenseGateStatusToString(DefenseGateStatus status);

class DefenseGateResult {
public:
    DefenseGateResult();

    static DefenseGateResult allowed();
    static DefenseGateResult blocked(std::string reason);

    bool isAllowed() const;
    DefenseGateStatus status() const;
    const std::string& reason() const;

private:
    DefenseGateStatus m_status;
    std::string m_reason;
};

/*
 * DefenseModeGuard enforces that blocked operations cannot proceed when
 * defense mode is ACTIVE. Each gate function returns a result that callers
 * must check before executing the guarded operation.
 *
 * Security principle:
 * Defense mode is a protocol-level gate. Any bypass — whether by CLI, test
 * helper, or production path — must be an explicit decision with audit trail.
 */
class DefenseModeGuard {
public:
    static DefenseGateResult checkTreasurySpend(
        economics::DefenseModeState state,
        const economics::DefenseModePolicy& policy
    );

    static DefenseGateResult checkExtraordinaryMint(
        economics::DefenseModeState state,
        const economics::DefenseModePolicy& policy
    );

    static DefenseGateResult checkExtraordinaryReward(
        economics::DefenseModeState state,
        const economics::DefenseModePolicy& policy
    );

    static DefenseGateResult checkGovernanceApproval(
        economics::DefenseModeState state
    );
};

} // namespace nodo::node

#endif
