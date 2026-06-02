#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_RECORD_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_RECORD_HPP

#include "economics/GovernanceVotingPolicy.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

enum class GovernanceVoteChoice {
    YES,
    NO,
    ABSTAIN
};

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice);
bool governanceVoteChoiceFromString(
    const std::string& value,
    GovernanceVoteChoice& out
);

class GovernanceVoteRecord {
public:
    GovernanceVoteRecord();

    GovernanceVoteRecord(
        std::string voteId,
        std::string governanceProposalId,
        std::string voterId,
        GovernanceVoteChoice voteChoice,
        utils::Amount votingPower,
        std::uint64_t castAtBlock,
        std::string votingPowerSource,
        std::string voteProof,
        std::string policyVersion
    );

    const std::string& voteId() const;
    const std::string& governanceProposalId() const;
    const std::string& voterId() const;
    GovernanceVoteChoice voteChoice() const;
    utils::Amount votingPower() const;
    std::uint64_t castAtBlock() const;
    const std::string& votingPowerSource() const;
    const std::string& voteProof() const;
    const std::string& policyVersion() const;

    bool isValid() const;
    std::string rejectionReason() const;

    bool isValidUnderPolicy(const GovernanceVotingPolicy& policy) const;
    std::string policyRejectionReason(const GovernanceVotingPolicy& policy) const;

    std::string serialize() const;

private:
    std::string m_voteId;
    std::string m_governanceProposalId;
    std::string m_voterId;
    GovernanceVoteChoice m_voteChoice;
    utils::Amount m_votingPower;
    std::uint64_t m_castAtBlock;
    std::string m_votingPowerSource;
    std::string m_voteProof;
    std::string m_policyVersion;
};

} // namespace nodo::economics

#endif
