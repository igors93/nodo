#include "economics/GovernanceVoteSetAudit.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace nodo::economics {

namespace {

bool sameVotingPolicy(
    const GovernanceVotingPolicy& left,
    const GovernanceVotingPolicy& right
) {
    return left.policyVersion() == right.policyVersion() &&
           left.quorumVotingPower() == right.quorumVotingPower() &&
           left.approvalThresholdBasisPoints() ==
               right.approvalThresholdBasisPoints() &&
           left.minimumVotingPower() == right.minimumVotingPower() &&
           left.allowAbstain() == right.allowAbstain() &&
           left.allowVoteReplacement() == right.allowVoteReplacement();
}

} // namespace

std::string governanceVoteSetAuditStatusToString(
    GovernanceVoteSetAuditStatus status
) {
    switch (status) {
        case GovernanceVoteSetAuditStatus::ACCEPTED:
            return "ACCEPTED";
        case GovernanceVoteSetAuditStatus::INVALID_VOTING_POLICY:
            return "INVALID_VOTING_POLICY";
        case GovernanceVoteSetAuditStatus::EMPTY_VOTE_SET:
            return "EMPTY_VOTE_SET";
        case GovernanceVoteSetAuditStatus::INVALID_VOTE_EVIDENCE:
            return "INVALID_VOTE_EVIDENCE";
        case GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH:
            return "PROPOSAL_MISMATCH";
        case GovernanceVoteSetAuditStatus::POLICY_VERSION_MISMATCH:
            return "POLICY_VERSION_MISMATCH";
        case GovernanceVoteSetAuditStatus::DUPLICATE_EVIDENCE_ID:
            return "DUPLICATE_EVIDENCE_ID";
        case GovernanceVoteSetAuditStatus::DUPLICATE_VOTE_ID:
            return "DUPLICATE_VOTE_ID";
        case GovernanceVoteSetAuditStatus::DUPLICATE_VOTER:
            return "DUPLICATE_VOTER";
        case GovernanceVoteSetAuditStatus::REPLACEMENT_NOT_IMPLEMENTED:
            return "REPLACEMENT_NOT_IMPLEMENTED";
        default:
            return "UNKNOWN";
    }
}

GovernanceVoteSetAuditResult::GovernanceVoteSetAuditResult()
    : m_accepted(false),
      m_status(GovernanceVoteSetAuditStatus::INVALID_VOTING_POLICY),
      m_failedAtIndex(0) {}

GovernanceVoteSetAuditResult GovernanceVoteSetAuditResult::accepted(
    std::vector<GovernanceVoteEvidence> canonicalVotes
) {
    GovernanceVoteSetAuditResult result;
    result.m_accepted = true;
    result.m_status = GovernanceVoteSetAuditStatus::ACCEPTED;
    result.m_canonicalVotes = std::move(canonicalVotes);
    return result;
}

GovernanceVoteSetAuditResult GovernanceVoteSetAuditResult::rejected(
    GovernanceVoteSetAuditStatus status,
    std::string reason,
    std::size_t failedAtIndex
) {
    GovernanceVoteSetAuditResult result;
    result.m_accepted = false;
    result.m_status = status;
    result.m_reason = std::move(reason);
    result.m_failedAtIndex = failedAtIndex;
    return result;
}

bool GovernanceVoteSetAuditResult::accepted() const {
    return m_accepted;
}

bool GovernanceVoteSetAuditResult::isValid() const {
    return m_accepted;
}

GovernanceVoteSetAuditStatus GovernanceVoteSetAuditResult::status() const {
    return m_status;
}

const std::string& GovernanceVoteSetAuditResult::reason() const {
    return m_reason;
}

std::size_t GovernanceVoteSetAuditResult::failedAtIndex() const {
    return m_failedAtIndex;
}

const std::vector<GovernanceVoteEvidence>&
GovernanceVoteSetAuditResult::canonicalVotes() const {
    return m_canonicalVotes;
}

GovernanceVoteSetAuditResult GovernanceVoteSetAudit::audit(
    const std::string& governanceProposalId,
    const GovernanceVotingPolicy& votingPolicy,
    const std::vector<GovernanceVoteEvidence>& voteEvidenceList
) {
    if (!votingPolicy.isValid()) {
        return GovernanceVoteSetAuditResult::rejected(
            GovernanceVoteSetAuditStatus::INVALID_VOTING_POLICY,
            "GovernanceVoteSetAudit: voting policy is invalid: " +
                votingPolicy.rejectionReason()
        );
    }

    if (governanceProposalId.empty()) {
        return GovernanceVoteSetAuditResult::rejected(
            GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH,
            "GovernanceVoteSetAudit: governanceProposalId must not be empty."
        );
    }

    if (voteEvidenceList.empty()) {
        return GovernanceVoteSetAuditResult::rejected(
            GovernanceVoteSetAuditStatus::EMPTY_VOTE_SET,
            "GovernanceVoteSetAudit: production lifecycle requires vote evidence."
        );
    }

    std::set<std::string> evidenceIds;
    std::set<std::string> voteIds;
    std::set<std::string> voterIds;

    for (std::size_t i = 0; i < voteEvidenceList.size(); ++i) {
        const GovernanceVoteEvidence& evidence = voteEvidenceList[i];

        if (!evidence.isValid()) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::INVALID_VOTE_EVIDENCE,
                "GovernanceVoteSetAudit: vote evidence is invalid: " +
                    evidence.rejectionReason(),
                i
            );
        }

        if (evidence.proposalEnvelope().governanceProposalId() !=
            governanceProposalId) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH,
                "GovernanceVoteSetAudit: evidence proposal does not match audit proposal.",
                i
            );
        }

        if (!sameVotingPolicy(evidence.votingPolicy(), votingPolicy)) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::POLICY_VERSION_MISMATCH,
                "GovernanceVoteSetAudit: evidence voting policy does not match audit policy.",
                i
            );
        }

        const GovernanceVoteRecord& vote = evidence.voteRecord();
        if (vote.governanceProposalId() != governanceProposalId) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH,
                "GovernanceVoteSetAudit: vote proposal does not match audit proposal.",
                i
            );
        }

        if (vote.policyVersion() != votingPolicy.policyVersion()) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::POLICY_VERSION_MISMATCH,
                "GovernanceVoteSetAudit: vote policyVersion does not match audit policy.",
                i
            );
        }

        if (!evidenceIds.insert(evidence.evidenceId()).second) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::DUPLICATE_EVIDENCE_ID,
                "GovernanceVoteSetAudit: duplicate evidenceId '" +
                    evidence.evidenceId() + "'.",
                i
            );
        }

        if (!voteIds.insert(vote.voteId()).second) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::DUPLICATE_VOTE_ID,
                "GovernanceVoteSetAudit: duplicate voteId '" + vote.voteId() + "'.",
                i
            );
        }

        if (!voterIds.insert(vote.voterId()).second) {
            if (votingPolicy.allowVoteReplacement()) {
                return GovernanceVoteSetAuditResult::rejected(
                    GovernanceVoteSetAuditStatus::REPLACEMENT_NOT_IMPLEMENTED,
                    "GovernanceVoteSetAudit: vote replacement is not implemented.",
                    i
                );
            }
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::DUPLICATE_VOTER,
                "GovernanceVoteSetAudit: duplicate voterId '" + vote.voterId() + "'.",
                i
            );
        }
    }

    std::vector<GovernanceVoteEvidence> canonical = voteEvidenceList;
    std::sort(
        canonical.begin(),
        canonical.end(),
        [](const GovernanceVoteEvidence& left, const GovernanceVoteEvidence& right) {
            return left.voteRecord().voteId() < right.voteRecord().voteId();
        }
    );

    return GovernanceVoteSetAuditResult::accepted(std::move(canonical));
}

GovernanceVoteSetAuditResult GovernanceVoteSetAudit::auditVotes(
    const GovernanceVotingPolicy& votingPolicy,
    const std::string& governanceProposalId,
    const std::vector<GovernanceVoteEvidence>& voteEvidenceList
) {
    return audit(governanceProposalId, votingPolicy, voteEvidenceList);
}

} // namespace nodo::economics
