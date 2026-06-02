#include "economics/DefenseModeState.hpp"

namespace nodo::economics {

std::string defenseModeStateToString(DefenseModeState state) {
    switch (state) {
        case DefenseModeState::INACTIVE: return "INACTIVE";
        case DefenseModeState::ACTIVE:   return "ACTIVE";
        default:                         return "UNKNOWN";
    }
}

bool defenseModeStateFromString(const std::string& s, DefenseModeState& out) {
    if (s == "INACTIVE") { out = DefenseModeState::INACTIVE; return true; }
    if (s == "ACTIVE")   { out = DefenseModeState::ACTIVE;   return true; }
    return false;
}

std::string defenseModeTriggerToString(DefenseModeTrigger trigger) {
    switch (trigger) {
        case DefenseModeTrigger::SUPPLY_DIVERGENCE:                    return "SUPPLY_DIVERGENCE";
        case DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT:               return "DOUBLE_SIGN_MASS_EVENT";
        case DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT:  return "UNAUTHORIZED_TREASURY_SPEND_ATTEMPT";
        case DefenseModeTrigger::CHAIN_AUDIT_FAILURE:                  return "CHAIN_AUDIT_FAILURE";
        case DefenseModeTrigger::STORAGE_CORRUPTION:                   return "STORAGE_CORRUPTION";
        case DefenseModeTrigger::OPERATOR_DECLARED:                    return "OPERATOR_DECLARED";
        case DefenseModeTrigger::GOVERNANCE_VOTED:                     return "GOVERNANCE_VOTED";
        default:                                                        return "UNKNOWN";
    }
}

DefenseModePolicy::DefenseModePolicy()
    : m_blockTreasurySpend(true),
      m_blockExtraordinaryMint(true),
      m_requireChainAuditBeforeExit(true),
      m_allowNormalTransactions(true) {}

DefenseModePolicy::DefenseModePolicy(
    bool blockTreasurySpend,
    bool blockExtraordinaryMint,
    bool requireChainAuditBeforeExit,
    bool allowNormalTransactions
)
    : m_blockTreasurySpend(blockTreasurySpend),
      m_blockExtraordinaryMint(blockExtraordinaryMint),
      m_requireChainAuditBeforeExit(requireChainAuditBeforeExit),
      m_allowNormalTransactions(allowNormalTransactions) {}

bool DefenseModePolicy::blockTreasurySpend() const { return m_blockTreasurySpend; }
bool DefenseModePolicy::blockExtraordinaryMint() const { return m_blockExtraordinaryMint; }
bool DefenseModePolicy::requireChainAuditBeforeExit() const { return m_requireChainAuditBeforeExit; }
bool DefenseModePolicy::allowNormalTransactions() const { return m_allowNormalTransactions; }

DefenseModePolicy DefenseModePolicy::defaultPolicy() {
    return DefenseModePolicy(true, true, true, true);
}

} // namespace nodo::economics
