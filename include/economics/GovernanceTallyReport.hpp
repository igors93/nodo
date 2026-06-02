#ifndef NODO_ECONOMICS_GOVERNANCE_TALLY_REPORT_HPP
#define NODO_ECONOMICS_GOVERNANCE_TALLY_REPORT_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

class GovernanceTallyReport {
public:
    GovernanceTallyReport();

    GovernanceTallyReport(
        std::string governanceProposalId,
        std::string policyVersion,
        std::uint64_t totalVotingPower,
        std::uint64_t yesVotingPower,
        std::uint64_t noVotingPower,
        std::uint64_t abstainVotingPower,
        std::uint64_t yesVoteCount,
        std::uint64_t noVoteCount,
        std::uint64_t abstainVoteCount,
        bool quorumMet,
        bool approvalThresholdMet,
        bool approved,
        std::string tallyProof
    );

    const std::string& governanceProposalId() const;
    const std::string& policyVersion() const;
    std::uint64_t totalVotingPower() const;
    std::uint64_t yesVotingPower() const;
    std::uint64_t noVotingPower() const;
    std::uint64_t abstainVotingPower() const;
    std::uint64_t yesVoteCount() const;
    std::uint64_t noVoteCount() const;
    std::uint64_t abstainVoteCount() const;
    bool quorumMet() const;
    bool approvalThresholdMet() const;
    bool approved() const;
    const std::string& tallyProof() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

    static std::string buildTallyProof(
        const std::string& governanceProposalId,
        const std::string& policyVersion,
        std::uint64_t totalVotingPower,
        std::uint64_t yesVotingPower,
        std::uint64_t noVotingPower,
        std::uint64_t abstainVotingPower,
        std::uint64_t yesVoteCount,
        std::uint64_t noVoteCount,
        std::uint64_t abstainVoteCount,
        bool quorumMet,
        bool approvalThresholdMet,
        bool approved
    );

private:
    std::string m_governanceProposalId;
    std::string m_policyVersion;
    std::uint64_t m_totalVotingPower;
    std::uint64_t m_yesVotingPower;
    std::uint64_t m_noVotingPower;
    std::uint64_t m_abstainVotingPower;
    std::uint64_t m_yesVoteCount;
    std::uint64_t m_noVoteCount;
    std::uint64_t m_abstainVoteCount;
    bool m_quorumMet;
    bool m_approvalThresholdMet;
    bool m_approved;
    std::string m_tallyProof;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
