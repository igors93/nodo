#ifndef NODO_ECONOMICS_DEFENSE_MODE_STATE_HPP
#define NODO_ECONOMICS_DEFENSE_MODE_STATE_HPP

#include <string>

namespace nodo::economics {

/*
 * DefenseModeState represents whether the network is operating in a
 * heightened-security defensive posture.
 *
 * Security principle:
 * When ACTIVE, treasury spend, extraordinary minting, and other economic
 * actions must be blocked until the network returns to INACTIVE through a
 * verified governance decision. Defense mode is a protocol-level gate, not
 * just an advisory flag.
 */
enum class DefenseModeState {
    INACTIVE,
    ACTIVE
};

std::string defenseModeStateToString(DefenseModeState state);

bool defenseModeStateFromString(
    const std::string& s,
    DefenseModeState& out
);

enum class DefenseModeTrigger {
    SUPPLY_DIVERGENCE,
    DOUBLE_SIGN_MASS_EVENT,
    UNAUTHORIZED_TREASURY_SPEND_ATTEMPT,
    CHAIN_AUDIT_FAILURE,
    STORAGE_CORRUPTION,
    OPERATOR_DECLARED,
    GOVERNANCE_VOTED
};

std::string defenseModeTriggerToString(DefenseModeTrigger trigger);

class DefenseModePolicy {
public:
    DefenseModePolicy();

    DefenseModePolicy(
        bool blockTreasurySpend,
        bool blockExtraordinaryMint,
        bool blockExtraordinaryRewards,
        bool requireChainAuditBeforeExit,
        bool allowNormalTransactions
    );

    bool blockTreasurySpend() const;
    bool blockExtraordinaryMint() const;
    bool blockExtraordinaryRewards() const;
    bool requireChainAuditBeforeExit() const;
    bool allowNormalTransactions() const;

    // Returns the default defense policy: block economic actions, allow normal txs.
    static DefenseModePolicy defaultPolicy();

private:
    bool m_blockTreasurySpend;
    bool m_blockExtraordinaryMint;
    bool m_blockExtraordinaryRewards;
    bool m_requireChainAuditBeforeExit;
    bool m_allowNormalTransactions;
};

} // namespace nodo::economics

#endif
