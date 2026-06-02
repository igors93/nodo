#ifndef NODO_ECONOMICS_GOVERNANCE_VOTE_RECORD_HPP
#define NODO_ECONOMICS_GOVERNANCE_VOTE_RECORD_HPP

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
        GovernanceVoteChoice choice,
        std::uint64_t votingPower,
        std::uint64_t votedAtBlock,
        std::string policyVersion
    );

    const std::string& voteId() const;
    const std::string& governanceProposalId() const;
    const std::string& voterId() const;
    GovernanceVoteChoice choice() const;
    std::uint64_t votingPower() const;
    std::uint64_t votedAtBlock() const;
    const std::string& policyVersion() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_voteId;
    std::string m_governanceProposalId;
    std::string m_voterId;
    GovernanceVoteChoice m_choice;
    std::uint64_t m_votingPower;
    std::uint64_t m_votedAtBlock;
    std::string m_policyVersion;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
