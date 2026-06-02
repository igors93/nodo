#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_RECORD_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_RECORD_HPP

#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleTransition.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVoteEvidence.hpp"
#include "economics/GovernanceVotingPolicy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

class GovernanceLifecycleRecord {
public:
    GovernanceLifecycleRecord();

    GovernanceLifecycleRecord(
        std::string lifecycleId,
        GovernanceProposalEnvelope proposalEnvelope,
        GovernancePolicy governancePolicy,
        GovernanceVotingPolicy votingPolicy,
        std::vector<GovernanceVoteEvidence> voteEvidenceList,
        GovernanceTallyReport tallyReport,
        GovernanceDecisionRecord decisionRecord,
        std::uint64_t createdAtBlock,
        std::uint64_t finalizedAtBlock,
        GovernanceLifecycleState declaredCurrentState,
        std::vector<GovernanceLifecycleTransition> transitionHistory
    );

    const std::string& lifecycleId() const;
    const GovernanceProposalEnvelope& proposalEnvelope() const;
    const GovernancePolicy& governancePolicy() const;
    const GovernanceVotingPolicy& votingPolicy() const;
    const std::vector<GovernanceVoteEvidence>& voteEvidenceList() const;
    const GovernanceTallyReport& tallyReport() const;
    const GovernanceDecisionRecord& decisionRecord() const;
    std::uint64_t createdAtBlock() const;
    std::uint64_t finalizedAtBlock() const;
    GovernanceLifecycleState declaredCurrentState() const;
    const std::vector<GovernanceLifecycleTransition>& transitionHistory() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_lifecycleId;
    GovernanceProposalEnvelope m_proposalEnvelope;
    GovernancePolicy m_governancePolicy;
    GovernanceVotingPolicy m_votingPolicy;
    std::vector<GovernanceVoteEvidence> m_voteEvidenceList;
    GovernanceTallyReport m_tallyReport;
    GovernanceDecisionRecord m_decisionRecord;
    std::uint64_t m_createdAtBlock;
    std::uint64_t m_finalizedAtBlock;
    GovernanceLifecycleState m_declaredCurrentState;
    std::vector<GovernanceLifecycleTransition> m_transitionHistory;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
