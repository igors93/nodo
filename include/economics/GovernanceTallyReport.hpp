#ifndef NODO_ECONOMICS_GOVERNANCE_TALLY_REPORT_HPP
#define NODO_ECONOMICS_GOVERNANCE_TALLY_REPORT_HPP

#include "economics/GovernanceVoteSetAudit.hpp"
#include "economics/GovernanceVotingPolicy.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
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
        std::uint64_t yesCount,
        std::uint64_t noCount,
        std::uint64_t abstainCount,
        bool quorumMet,
        bool approvalThresholdMet,
        bool approved,
        std::string tallyProof
    );

    static GovernanceTallyReport build(
        const GovernanceVoteSetAuditResult& auditResult,
        const GovernanceVotingPolicy& votingPolicy
    );

    const std::string& governanceProposalId() const;
    const std::string& policyVersion() const;
    utils::Amount totalVotingPower() const;
    utils::Amount yesVotingPower() const;
    utils::Amount noVotingPower() const;
    utils::Amount abstainVotingPower() const;
    std::size_t voteCount() const;
    std::size_t yesCount() const;
    std::size_t noCount() const;
    std::size_t abstainCount() const;
    std::size_t yesVoteCount() const;
    std::size_t noVoteCount() const;
    std::size_t abstainVoteCount() const;
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
        std::uint64_t yesCount,
        std::uint64_t noCount,
        std::uint64_t abstainCount,
        bool quorumMet,
        bool approvalThresholdMet,
        bool approved
    );

private:
    std::string m_governanceProposalId;
    std::string m_policyVersion;
    utils::Amount m_totalVotingPower;
    utils::Amount m_yesVotingPower;
    utils::Amount m_noVotingPower;
    utils::Amount m_abstainVotingPower;
    std::size_t m_voteCount;
    std::size_t m_yesCount;
    std::size_t m_noCount;
    std::size_t m_abstainCount;
    bool m_quorumMet;
    bool m_approvalThresholdMet;
    bool m_approved;
    std::string m_tallyProof;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
