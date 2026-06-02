#ifndef NODO_ECONOMICS_GOVERNANCE_POLICY_HPP
#define NODO_ECONOMICS_GOVERNANCE_POLICY_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GovernancePolicy defines the rules under which a governance lifecycle
 * may produce a TreasuryApproval.
 *
 * Security principle:
 * No TreasuryApproval may be produced by GovernanceApprovalBridge unless
 * the governance lifecycle satisfies the policy's review period and
 * timelock requirements. A policy with an empty policyVersion is invalid.
 */
class GovernancePolicy {
public:
    GovernancePolicy();

    GovernancePolicy(
        std::string policyVersion,
        std::uint64_t reviewPeriodBlocks,
        std::uint64_t decisionTimelockBlocks,
        bool requireDecisionProof,
        bool allowEmergencyApproval
    );

    const std::string& policyVersion() const;
    std::uint64_t reviewPeriodBlocks() const;
    std::uint64_t decisionTimelockBlocks() const;
    bool requireDecisionProof() const;
    bool allowEmergencyApproval() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_policyVersion;
    std::uint64_t m_reviewPeriodBlocks;
    std::uint64_t m_decisionTimelockBlocks;
    bool m_requireDecisionProof;
    bool m_allowEmergencyApproval;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
