#ifndef NODO_ECONOMICS_TREASURY_POLICY_HPP
#define NODO_ECONOMICS_TREASURY_POLICY_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * TreasuryPolicy governs the rules under which the protocol treasury may spend.
 *
 * Security principle:
 * A zero maxSpendPerEpoch or maxSpendPerProposal means no spending is
 * permitted regardless of approvals. Enforcement is by TreasurySpendValidator.
 */
class TreasuryPolicy {
public:
    TreasuryPolicy();

    TreasuryPolicy(
        std::string policyVersion,
        utils::Amount maxSpendPerEpoch,
        utils::Amount maxSpendPerProposal,
        std::uint64_t timelockBlocks,
        bool requireApproval,
        bool allowSpendingWhenLocked
    );

    const std::string& policyVersion() const;
    utils::Amount maxSpendPerEpoch() const;
    utils::Amount maxSpendPerProposal() const;
    std::uint64_t timelockBlocks() const;
    bool requireApproval() const;
    bool allowSpendingWhenLocked() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_policyVersion;
    utils::Amount m_maxSpendPerEpoch;
    utils::Amount m_maxSpendPerProposal;
    std::uint64_t m_timelockBlocks;
    bool m_requireApproval;
    bool m_allowSpendingWhenLocked;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
