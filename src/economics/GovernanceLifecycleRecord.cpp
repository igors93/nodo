#include "economics/GovernanceLifecycleRecord.hpp"

#include <cstddef>
#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceLifecycleRecord::GovernanceLifecycleRecord()
    : m_createdAtBlock(0),
      m_finalizedAtBlock(0),
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
    std::uint64_t finalizedAtBlock
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
