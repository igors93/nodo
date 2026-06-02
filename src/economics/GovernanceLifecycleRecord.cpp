#include "economics/GovernanceLifecycleRecord.hpp"

#include "economics/GovernanceLifecycleTransitionAudit.hpp"

#include <cstddef>
#include <sstream>
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

GovernanceLifecycleRecord::GovernanceLifecycleRecord()
    : m_createdAtBlock(0),
      m_finalizedAtBlock(0),
      m_declaredCurrentState(GovernanceLifecycleState::DRAFT),
      m_valid(false),
      m_rejectionReason("GovernanceLifecycleRecord: default-constructed.") {}

GovernanceLifecycleRecord::GovernanceLifecycleRecord(
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
)
    : m_lifecycleId(std::move(lifecycleId)),
      m_proposalEnvelope(std::move(proposalEnvelope)),
      m_governancePolicy(std::move(governancePolicy)),
      m_votingPolicy(std::move(votingPolicy)),
      m_voteEvidenceList(std::move(voteEvidenceList)),
      m_tallyReport(std::move(tallyReport)),
      m_decisionRecord(std::move(decisionRecord)),
      m_createdAtBlock(createdAtBlock),
      m_finalizedAtBlock(finalizedAtBlock),
      m_declaredCurrentState(declaredCurrentState),
      m_transitionHistory(std::move(transitionHistory)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_lifecycleId.empty()) {
        m_rejectionReason = "GovernanceLifecycleRecord: lifecycleId must not be empty.";
        return;
    }

    if (!m_proposalEnvelope.isValid()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: proposalEnvelope is invalid: " +
            m_proposalEnvelope.rejectionReason();
        return;
    }

    if (!m_governancePolicy.isValid()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: governancePolicy is invalid: " +
            m_governancePolicy.rejectionReason();
        return;
    }

    if (!m_votingPolicy.isValid()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: votingPolicy is invalid: " +
            m_votingPolicy.rejectionReason();
        return;
    }

    if (m_voteEvidenceList.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: voteEvidenceList must not be empty.";
        return;
    }

    for (std::size_t index = 0; index < m_voteEvidenceList.size(); ++index) {
        if (!m_voteEvidenceList[index].isValid()) {
            m_rejectionReason =
                "GovernanceLifecycleRecord: vote evidence at index " +
                std::to_string(index) + " is invalid: " +
                m_voteEvidenceList[index].rejectionReason();
            return;
        }

        if (m_voteEvidenceList[index].proposalEnvelope().serialize() !=
            m_proposalEnvelope.serialize()) {
            m_rejectionReason =
                "GovernanceLifecycleRecord: vote evidence at index " +
                std::to_string(index) +
                " is bound to a different proposal envelope.";
            return;
        }

        if (!sameVotingPolicy(
                m_voteEvidenceList[index].votingPolicy(),
                m_votingPolicy)) {
            m_rejectionReason =
                "GovernanceLifecycleRecord: vote evidence at index " +
                std::to_string(index) +
                " is bound to a different voting policy.";
            return;
        }
    }

    if (!m_tallyReport.isValid()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: tallyReport is invalid: " +
            m_tallyReport.rejectionReason();
        return;
    }

    if (!m_decisionRecord.isValid()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: decisionRecord is invalid: " +
            m_decisionRecord.rejectionReason();
        return;
    }

    if (m_governancePolicy.policyVersion() != m_votingPolicy.policyVersion() ||
        m_proposalEnvelope.governancePolicyVersion() != m_governancePolicy.policyVersion()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: governance, voting, and envelope policy "
            "versions must match.";
        return;
    }

    if (m_tallyReport.governanceProposalId() !=
        m_proposalEnvelope.governanceProposalId()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: tally proposal id does not match envelope.";
        return;
    }

    if (m_decisionRecord.governanceProposalId() !=
        m_proposalEnvelope.governanceProposalId()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: decision proposal id does not match envelope.";
        return;
    }

    if (m_tallyReport.policyVersion() != m_votingPolicy.policyVersion() ||
        m_decisionRecord.policyVersion() != m_votingPolicy.policyVersion()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: tally/decision policy version mismatch.";
        return;
    }

    if (m_decisionRecord.proposalType() != m_proposalEnvelope.proposalType()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: decision proposal type does not match envelope.";
        return;
    }

    if (m_createdAtBlock == 0) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: createdAtBlock must be positive.";
        return;
    }

    if (m_finalizedAtBlock < m_createdAtBlock) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: finalizedAtBlock must be >= createdAtBlock.";
        return;
    }

    // Validate transition history and verify declared state matches rebuilt final state.
    if (m_transitionHistory.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: transitionHistory must not be empty.";
        return;
    }

    const GovernanceLifecycleTransitionAuditResult transitionAudit =
        GovernanceLifecycleTransitionAudit::audit(
            m_transitionHistory,
            m_proposalEnvelope.governanceProposalId(),
            m_governancePolicy.policyVersion()
        );

    if (!transitionAudit.accepted()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: transition audit failed: " +
            transitionAudit.reason();
        return;
    }

    if (m_declaredCurrentState != transitionAudit.finalState()) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: declaredCurrentState='" +
            governanceLifecycleStateToString(m_declaredCurrentState) +
            "' does not match rebuilt final state='" +
            governanceLifecycleStateToString(transitionAudit.finalState()) + "'.";
        return;
    }

    // Votes require the lifecycle to have reached at least VOTING.
    // A decided lifecycle (DECIDED_APPROVED / DECIDED_REJECTED) necessarily passed VOTING.
    if (!isDecidedGovernanceState(m_declaredCurrentState) &&
        m_declaredCurrentState != GovernanceLifecycleState::APPROVAL_PRODUCED &&
        m_declaredCurrentState != GovernanceLifecycleState::EXECUTED) {
        m_rejectionReason =
            "GovernanceLifecycleRecord: vote evidence is only valid for lifecycles "
            "that reached a decided state. Current state: '" +
            governanceLifecycleStateToString(m_declaredCurrentState) + "'.";
        return;
    }

    m_valid = true;
}

const std::string& GovernanceLifecycleRecord::lifecycleId() const {
    return m_lifecycleId;
}
const GovernanceProposalEnvelope& GovernanceLifecycleRecord::proposalEnvelope() const {
    return m_proposalEnvelope;
}
const GovernancePolicy& GovernanceLifecycleRecord::governancePolicy() const {
    return m_governancePolicy;
}
const GovernanceVotingPolicy& GovernanceLifecycleRecord::votingPolicy() const {
    return m_votingPolicy;
}
const std::vector<GovernanceVoteEvidence>&
GovernanceLifecycleRecord::voteEvidenceList() const {
    return m_voteEvidenceList;
}
const GovernanceTallyReport& GovernanceLifecycleRecord::tallyReport() const {
    return m_tallyReport;
}
const GovernanceDecisionRecord& GovernanceLifecycleRecord::decisionRecord() const {
    return m_decisionRecord;
}
std::uint64_t GovernanceLifecycleRecord::createdAtBlock() const {
    return m_createdAtBlock;
}
std::uint64_t GovernanceLifecycleRecord::finalizedAtBlock() const {
    return m_finalizedAtBlock;
}
GovernanceLifecycleState GovernanceLifecycleRecord::declaredCurrentState() const {
    return m_declaredCurrentState;
}
const std::vector<GovernanceLifecycleTransition>&
GovernanceLifecycleRecord::transitionHistory() const {
    return m_transitionHistory;
}
bool GovernanceLifecycleRecord::isValid() const { return m_valid; }
const std::string& GovernanceLifecycleRecord::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceLifecycleRecord::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceLifecycleRecord{"
        << "lifecycleId=" << m_lifecycleId
        << ";createdAtBlock=" << m_createdAtBlock
        << ";finalizedAtBlock=" << m_finalizedAtBlock
        << ";declaredCurrentState=" << governanceLifecycleStateToString(m_declaredCurrentState)
        << ";transitionCount=" << m_transitionHistory.size()
        << ";proposalEnvelope=" << m_proposalEnvelope.serialize()
        << ";governancePolicy=" << m_governancePolicy.serialize()
        << ";votingPolicy=" << m_votingPolicy.serialize()
        << ";voteEvidenceCount=" << m_voteEvidenceList.size()
        << ";tallyReport=" << m_tallyReport.serialize()
        << ";decisionRecord=" << m_decisionRecord.serialize()
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
