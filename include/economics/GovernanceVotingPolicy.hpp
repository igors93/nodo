#ifndef NODO_ECONOMICS_GOVERNANCE_VOTING_POLICY_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTING_POLICY_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

class GovernanceVotingPolicy {
public:
    GovernanceVotingPolicy();

    GovernanceVotingPolicy(
        std::string policyVersion,
        utils::Amount quorumVotingPower,
        std::uint32_t approvalThresholdBasisPoints,
        utils::Amount minimumVotingPower,
        bool allowAbstain,
        bool allowVoteReplacement
    );

    const std::string& policyVersion() const;
    utils::Amount quorumVotingPower() const;
    std::uint32_t approvalThresholdBasisPoints() const;
    utils::Amount minimumVotingPower() const;
    bool allowAbstain() const;
    bool allowVoteReplacement() const;

    bool isValid() const;
    std::string rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_policyVersion;
    utils::Amount m_quorumVotingPower;
    std::uint32_t m_approvalThresholdBasisPoints;
    utils::Amount m_minimumVotingPower;
    bool m_allowAbstain;
    bool m_allowVoteReplacement;
};

} // namespace nodo::economics

#endif
