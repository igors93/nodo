#include "node/DefenseModeGuard.hpp"

#include <utility>

namespace nodo::node {

std::string defenseGateStatusToString(DefenseGateStatus status) {
    switch (status) {
        case DefenseGateStatus::ALLOWED:                     return "ALLOWED";
        case DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE: return "BLOCKED_DEFENSE_MODE_ACTIVE";
        default:                                              return "UNKNOWN";
    }
}

DefenseGateResult::DefenseGateResult()
    : m_status(DefenseGateStatus::ALLOWED) {}

DefenseGateResult DefenseGateResult::allowed() {
    DefenseGateResult r;
    r.m_status = DefenseGateStatus::ALLOWED;
    return r;
}

DefenseGateResult DefenseGateResult::blocked(std::string reason) {
    DefenseGateResult r;
    r.m_status = DefenseGateStatus::BLOCKED_DEFENSE_MODE_ACTIVE;
    r.m_reason = std::move(reason);
    return r;
}

bool DefenseGateResult::isAllowed() const {
    return m_status == DefenseGateStatus::ALLOWED;
}
DefenseGateStatus DefenseGateResult::status() const { return m_status; }
const std::string& DefenseGateResult::reason() const { return m_reason; }

DefenseGateResult DefenseModeGuard::checkTreasurySpend(
    economics::DefenseModeState state,
    const economics::DefenseModePolicy& policy
) {
    if (state == economics::DefenseModeState::ACTIVE && policy.blockTreasurySpend()) {
        return DefenseGateResult::blocked(
            "DefenseModeGuard: treasury spend is blocked while defense mode is ACTIVE."
        );
    }
    return DefenseGateResult::allowed();
}

DefenseGateResult DefenseModeGuard::checkExtraordinaryMint(
    economics::DefenseModeState state,
    const economics::DefenseModePolicy& policy
) {
    if (state == economics::DefenseModeState::ACTIVE && policy.blockExtraordinaryMint()) {
        return DefenseGateResult::blocked(
            "DefenseModeGuard: extraordinary mint is blocked while defense mode is ACTIVE."
        );
    }
    return DefenseGateResult::allowed();
}

DefenseGateResult DefenseModeGuard::checkGovernanceApproval(
    economics::DefenseModeState state
) {
    if (state == economics::DefenseModeState::ACTIVE) {
        return DefenseGateResult::blocked(
            "DefenseModeGuard: governance approval for treasury is blocked while defense mode is ACTIVE."
        );
    }
    return DefenseGateResult::allowed();
}

} // namespace nodo::node
