#include "economics/GovernanceVoteSetAudit.hpp"

#include <set>
#include <utility>

namespace nodo::economics {

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
        case GovernanceVoteSetAuditStatus::DUPLICATE_VOTE_ID:
            return "DUPLICATE_VOTE_ID";
        case GovernanceVoteSetAuditStatus::DUPLICATE_VOTER:
            return "DUPLICATE_VOTER";
        case GovernanceVoteSetAuditStatus::ABSTAIN_NOT_ALLOWED:
            return "ABSTAIN_NOT_ALLOWED";
        case GovernanceVoteSetAuditStatus::TALLY_BUILD_FAILED:
            return "TALLY_BUILD_FAILED";
        default:
            return "UNKNOWN";
    }
}

GovernanceVoteSetAuditResult::GovernanceVoteSetAuditResult()
    : m_accepted(false),
      m_status(GovernanceVoteSetAuditStatus::INVALID_VOTING_POLICY),
      m_failedAtIndex(0),
      m_tallyReport() {}

GovernanceVoteSetAuditResult GovernanceVoteSetAuditResult::accepted(
    GovernanceTallyReport tallyReport
) {
    GovernanceVoteSetAuditResult result;
    result.m_accepted = true;
    result.m_status = GovernanceVoteSetAuditStatus::ACCEPTED;
    result.m_tallyReport = std::move(tallyReport);
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

bool GovernanceVoteSetAuditResult::accepted() const { return m_accepted; }
GovernanceVoteSetAuditStatus GovernanceVoteSetAuditResult::status() const {
    return m_status;
}
const std::string& GovernanceVoteSetAuditResult::reason() const {
    return m_reason;
}
std::size_t GovernanceVoteSetAuditResult::failedAtIndex() const {
    return m_failedAtIndex;
}
const GovernanceTallyReport& GovernanceVoteSetAuditResult::tallyReport() const {
    return m_tallyReport;
}

GovernanceVoteSetAuditResult GovernanceVoteSetAudit::auditVotes(
    const GovernanceVotingPolicy& votingPolicy,
    const std::string& governanceProposalId,
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

    std::set<std::string> voteIds;
    std::set<std::string> voterIds;
    std::uint64_t yesPower = 0;
    std::uint64_t noPower = 0;
    std::uint64_t abstainPower = 0;
    std::uint64_t yesCount = 0;
    std::uint64_t noCount = 0;
    std::uint64_t abstainCount = 0;

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

        const GovernanceVoteRecord& vote = evidence.voteRecord();

        if (vote.governanceProposalId() != governanceProposalId) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH,
                "GovernanceVoteSetAudit: vote proposal '" +
                vote.governanceProposalId() + "' does not match lifecycle proposal '" +
                governanceProposalId + "'.",
                i
            );
        }

        if (vote.policyVersion() != votingPolicy.policyVersion()) {
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::POLICY_VERSION_MISMATCH,
                "GovernanceVoteSetAudit: vote policyVersion '" +
                vote.policyVersion() + "' does not match voting policy '" +
                votingPolicy.policyVersion() + "'.",
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
            return GovernanceVoteSetAuditResult::rejected(
                GovernanceVoteSetAuditStatus::DUPLICATE_VOTER,
                "GovernanceVoteSetAudit: duplicate voterId '" + vote.voterId() +
                "' for proposal '" + governanceProposalId + "'.",
                i
            );
        }

        switch (vote.choice()) {
            case GovernanceVoteChoice::YES:
                yesPower += vote.votingPower();
                ++yesCount;
                break;
            case GovernanceVoteChoice::NO:
                noPower += vote.votingPower();
                ++noCount;
                break;
            case GovernanceVoteChoice::ABSTAIN:
                if (!votingPolicy.allowAbstain()) {
                    return GovernanceVoteSetAuditResult::rejected(
                        GovernanceVoteSetAuditStatus::ABSTAIN_NOT_ALLOWED,
                        "GovernanceVoteSetAudit: abstain vote is not allowed.",
                        i
                    );
                }
                abstainPower += vote.votingPower();
                ++abstainCount;
                break;
        }
    }

    const std::uint64_t totalPower = yesPower + noPower + abstainPower;
    const bool quorumMet = totalPower >= votingPolicy.quorumThresholdPower();
    const bool approvalMet = yesPower >= votingPolicy.approvalThresholdPower();
    const bool approved = quorumMet && approvalMet;

    const std::string proof = GovernanceTallyReport::buildTallyProof(
        governanceProposalId,
        votingPolicy.policyVersion(),
        totalPower,
        yesPower,
        noPower,
        abstainPower,
        yesCount,
        noCount,
        abstainCount,
        quorumMet,
        approvalMet,
        approved
    );

    GovernanceTallyReport tally(
        governanceProposalId,
        votingPolicy.policyVersion(),
        totalPower,
        yesPower,
        noPower,
        abstainPower,
        yesCount,
        noCount,
        abstainCount,
        quorumMet,
        approvalMet,
        approved,
        proof
    );

    if (!tally.isValid()) {
        return GovernanceVoteSetAuditResult::rejected(
            GovernanceVoteSetAuditStatus::TALLY_BUILD_FAILED,
            "GovernanceVoteSetAudit: rebuilt tally is invalid: " +
            tally.rejectionReason()
        );
    }

    return GovernanceVoteSetAuditResult::accepted(std::move(tally));
}

} // namespace nodo::economics
