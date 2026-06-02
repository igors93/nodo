#ifndef NODO_ECONOMICS_GOVERNANCE_DECISION_BUILDER_HPP
#define NODO_ECONOMICS_GOVERNANCE_DECISION_BUILDER_HPP

#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVotingPolicy.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

enum class GovernanceDecisionBuildStatus {
    BUILT,
    INVALID_ENVELOPE,
    INVALID_VOTING_POLICY,
    INVALID_TALLY,
    PROPOSAL_MISMATCH,
    POLICY_VERSION_MISMATCH,
    INVALID_DECISION_INPUT,
    DECISION_BUILD_FAILED
};

std::string governanceDecisionBuildStatusToString(
    GovernanceDecisionBuildStatus status
);

class GovernanceDecisionBuildResult {
public:
    GovernanceDecisionBuildResult();

    static GovernanceDecisionBuildResult built(GovernanceDecisionRecord decision);

    static GovernanceDecisionBuildResult rejected(
        GovernanceDecisionBuildStatus status,
        std::string reason
    );

    bool built() const;
    bool isBuilt() const;
    GovernanceDecisionBuildStatus status() const;
    const std::string& reason() const;
    const GovernanceDecisionRecord& decisionRecord() const;
    const GovernanceDecisionRecord& decision() const;

private:
    bool m_built;
    GovernanceDecisionBuildStatus m_status;
    std::string m_reason;
    GovernanceDecisionRecord m_decisionRecord;
};

class GovernanceDecisionBuilder {
public:
    static GovernanceDecisionBuildResult buildDecision(
        const GovernanceProposalEnvelope& envelope,
        const GovernanceVotingPolicy& votingPolicy,
        const GovernanceTallyReport& tallyReport,
        std::uint64_t decidedAtBlock,
        const std::string& decisionMaker
    );

    static std::string buildDecisionId(
        const std::string& governanceProposalId,
        std::uint64_t decidedAtBlock
    );

    static std::string buildDecisionProof(
        const std::string& governanceProposalId,
        const std::string& decisionId,
        GovernanceDecisionStatus decisionStatus,
        const std::string& policyVersion,
        const std::string& tallyProof,
        std::uint64_t decidedAtBlock,
        const std::string& decisionMaker
    );
};

} // namespace nodo::economics

#endif
