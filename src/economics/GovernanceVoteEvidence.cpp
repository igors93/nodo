#include "economics/GovernanceVoteEvidence.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceVoteEvidence::GovernanceVoteEvidence() = default;

GovernanceVoteEvidence::GovernanceVoteEvidence(
    std::string evidenceId,
    GovernanceProposalEnvelope proposalEnvelope,
    GovernanceVotingPolicy votingPolicy,
    GovernanceVoteRecord voteRecord
)
    : m_evidenceId(std::move(evidenceId)),
      m_proposalEnvelope(std::move(proposalEnvelope)),
      m_votingPolicy(std::move(votingPolicy)),
      m_voteRecord(std::move(voteRecord)) {}

const std::string& GovernanceVoteEvidence::evidenceId() const {
    return m_evidenceId;
}

const GovernanceProposalEnvelope& GovernanceVoteEvidence::proposalEnvelope() const {
    return m_proposalEnvelope;
}

const GovernanceVotingPolicy& GovernanceVoteEvidence::votingPolicy() const {
    return m_votingPolicy;
}

const GovernanceVoteRecord& GovernanceVoteEvidence::voteRecord() const {
    return m_voteRecord;
}

bool GovernanceVoteEvidence::isValid() const {
    return rejectionReason().empty();
}

std::string GovernanceVoteEvidence::rejectionReason() const {
    if (m_evidenceId.empty()) {
        return "GovernanceVoteEvidence rejected: evidenceId is empty.";
    }
    if (!m_proposalEnvelope.isValid()) {
        return "GovernanceVoteEvidence rejected: proposalEnvelope is invalid: " +
               m_proposalEnvelope.rejectionReason();
    }
    if (!m_votingPolicy.isValid()) {
        return "GovernanceVoteEvidence rejected: votingPolicy is invalid: " +
               m_votingPolicy.rejectionReason();
    }
    if (!m_voteRecord.isValid()) {
        return "GovernanceVoteEvidence rejected: voteRecord is invalid: " +
               m_voteRecord.rejectionReason();
    }
    if (m_voteRecord.governanceProposalId() !=
        m_proposalEnvelope.governanceProposalId()) {
        return "GovernanceVoteEvidence rejected: vote proposal does not match envelope.";
    }
    if (m_voteRecord.policyVersion() != m_votingPolicy.policyVersion()) {
        return "GovernanceVoteEvidence rejected: vote policyVersion does not match votingPolicy.";
    }
    if (m_voteRecord.castAtBlock() < m_proposalEnvelope.submittedAtBlock()) {
        return "GovernanceVoteEvidence rejected: vote was cast before proposal submission.";
    }

    const std::string policyReason =
        m_voteRecord.policyRejectionReason(m_votingPolicy);
    if (!policyReason.empty()) {
        return "GovernanceVoteEvidence rejected: " + policyReason;
    }

    if (!GovernanceVoteProof::verify(m_voteRecord)) {
        return "GovernanceVoteEvidence rejected: voteProof does not match vote record.";
    }

    return "";
}

std::string GovernanceVoteEvidence::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVoteEvidence{"
        << "evidenceId=" << m_evidenceId
        << ";proposalId=" << m_proposalEnvelope.governanceProposalId()
        << ";policyVersion=" << m_votingPolicy.policyVersion()
        << ";voteRecord=" << m_voteRecord.serialize()
        << "}";
    return oss.str();
}

} // namespace nodo::economics
