#ifndef NODO_ECONOMICS_GOVERNANCE_VOTING_POLICY_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTING_POLICY_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

class GovernanceVotingPolicy {
public:
    GovernanceVotingPolicy();

    GovernanceVotingPolicy(
        std::string policyVersion,
        std::uint64_t quorumThresholdPower,
        std::uint64_t approvalThresholdPower,
        bool allowAbstain,
        bool requireVoteProof
    );

    const std::string& policyVersion() const;
    std::uint64_t quorumThresholdPower() const;
    std::uint64_t approvalThresholdPower() const;
    bool allowAbstain() const;
    bool requireVoteProof() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_policyVersion;
    std::uint64_t m_quorumThresholdPower;
    std::uint64_t m_approvalThresholdPower;
    bool m_allowAbstain;
    bool m_requireVoteProof;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
