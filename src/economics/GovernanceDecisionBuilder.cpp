#include "economics/GovernanceDecisionBuilder.hpp"

#include <utility>

namespace nodo::economics {

std::string governanceDecisionBuildStatusToString(
    GovernanceDecisionBuildStatus status
) {
    switch (status) {
        case GovernanceDecisionBuildStatus::BUILT:
            return "BUILT";
        case GovernanceDecisionBuildStatus::INVALID_ENVELOPE:
            return "INVALID_ENVELOPE";
        case GovernanceDecisionBuildStatus::INVALID_VOTING_POLICY:
            return "INVALID_VOTING_POLICY";
        case GovernanceDecisionBuildStatus::INVALID_TALLY:
            return "INVALID_TALLY";
        case GovernanceDecisionBuildStatus::PROPOSAL_MISMATCH:
            return "PROPOSAL_MISMATCH";
        case GovernanceDecisionBuildStatus::POLICY_VERSION_MISMATCH:
            return "POLICY_VERSION_MISMATCH";
        case GovernanceDecisionBuildStatus::INVALID_DECISION_INPUT:
            return "INVALID_DECISION_INPUT";
        case GovernanceDecisionBuildStatus::DECISION_BUILD_FAILED:
            return "DECISION_BUILD_FAILED";
        default:
            return "UNKNOWN";
    }
}

GovernanceDecisionBuildResult::GovernanceDecisionBuildResult()
    : m_built(false),
      m_status(GovernanceDecisionBuildStatus::INVALID_DECISION_INPUT),
      m_decisionRecord() {}

GovernanceDecisionBuildResult GovernanceDecisionBuildResult::built(
    GovernanceDecisionRecord decision
) {
    GovernanceDecisionBuildResult result;
    result.m_built = true;
    result.m_status = GovernanceDecisionBuildStatus::BUILT;
    result.m_decisionRecord = std::move(decision);
    return result;
}

GovernanceDecisionBuildResult GovernanceDecisionBuildResult::rejected(
    GovernanceDecisionBuildStatus status,
    std::string reason
) {
    GovernanceDecisionBuildResult result;
    result.m_built = false;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

bool GovernanceDecisionBuildResult::built() const { return m_built; }
GovernanceDecisionBuildStatus GovernanceDecisionBuildResult::status() const {
    return m_status;
}
const std::string& GovernanceDecisionBuildResult::reason() const {
    return m_reason;
}
const GovernanceDecisionRecord& GovernanceDecisionBuildResult::decisionRecord() const {
    return m_decisionRecord;
}

GovernanceDecisionBuildResult GovernanceDecisionBuilder::buildDecision(
    const GovernanceProposalEnvelope& envelope,
    const GovernanceVotingPolicy& votingPolicy,
    const GovernanceTallyReport& tallyReport,
    std::uint64_t decidedAtBlock,
    const std::string& decisionMaker
) {
    if (!envelope.isValid()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::INVALID_ENVELOPE,
            "GovernanceDecisionBuilder: envelope is invalid: " +
            envelope.rejectionReason()
        );
    }

    if (!votingPolicy.isValid()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::INVALID_VOTING_POLICY,
            "GovernanceDecisionBuilder: voting policy is invalid: " +
            votingPolicy.rejectionReason()
        );
    }

    if (!tallyReport.isValid()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::INVALID_TALLY,
            "GovernanceDecisionBuilder: tally is invalid: " +
            tallyReport.rejectionReason()
        );
    }

    if (tallyReport.governanceProposalId() != envelope.governanceProposalId()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::PROPOSAL_MISMATCH,
            "GovernanceDecisionBuilder: tally proposal does not match envelope."
        );
    }

    if (tallyReport.policyVersion() != votingPolicy.policyVersion() ||
        envelope.governancePolicyVersion() != votingPolicy.policyVersion()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::POLICY_VERSION_MISMATCH,
            "GovernanceDecisionBuilder: policy versions do not match."
        );
    }

    if (decidedAtBlock == 0 || decisionMaker.empty()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::INVALID_DECISION_INPUT,
            "GovernanceDecisionBuilder: decidedAtBlock and decisionMaker are required."
        );
    }

    const GovernanceDecisionStatus status =
        tallyReport.approved()
            ? GovernanceDecisionStatus::APPROVED
            : GovernanceDecisionStatus::REJECTED;

    const std::string decisionId =
        buildDecisionId(envelope.governanceProposalId(), decidedAtBlock);

    const std::string decisionProof = buildDecisionProof(
        envelope.governanceProposalId(),
        decisionId,
        status,
        votingPolicy.policyVersion(),
        tallyReport.tallyProof(),
        decidedAtBlock,
        decisionMaker
    );

    GovernanceDecisionRecord decision(
        decisionId,
        envelope.governanceProposalId(),
        envelope.proposalType(),
        status,
        decidedAtBlock,
        decisionMaker,
        decisionProof,
        votingPolicy.policyVersion()
    );

    if (!decision.isValid()) {
        return GovernanceDecisionBuildResult::rejected(
            GovernanceDecisionBuildStatus::DECISION_BUILD_FAILED,
            "GovernanceDecisionBuilder: decision record is invalid: " +
            decision.rejectionReason()
        );
    }

    return GovernanceDecisionBuildResult::built(std::move(decision));
}

std::string GovernanceDecisionBuilder::buildDecisionId(
    const std::string& governanceProposalId,
    std::uint64_t decidedAtBlock
) {
    return "gov-decision:" + governanceProposalId + ":" +
           std::to_string(decidedAtBlock);
}

std::string GovernanceDecisionBuilder::buildDecisionProof(
    const std::string& governanceProposalId,
    const std::string& decisionId,
    GovernanceDecisionStatus decisionStatus,
    const std::string& policyVersion,
    const std::string& tallyProof,
    std::uint64_t decidedAtBlock,
    const std::string& decisionMaker
) {
    return "governance-decision:"
        + governanceProposalId + ":"
        + decisionId + ":"
        + governanceDecisionStatusToString(decisionStatus) + ":"
        + policyVersion + ":"
        + tallyProof + ":"
        + std::to_string(decidedAtBlock) + ":"
        + decisionMaker;
}

} // namespace nodo::economics
