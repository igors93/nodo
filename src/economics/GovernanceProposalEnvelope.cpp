#include "economics/GovernanceProposalEnvelope.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceProposalEnvelope::GovernanceProposalEnvelope()
    : m_submittedAtBlock(0),
      m_valid(false),
      m_rejectionReason("GovernanceProposalEnvelope: default-constructed.") {}

GovernanceProposalEnvelope::GovernanceProposalEnvelope(
    std::string governanceProposalId,
    std::string proposalType,
    TreasuryProposal treasuryProposal,
    std::uint64_t submittedAtBlock,
    std::string submittedBy,
    std::string governancePolicyVersion,
    std::string summaryHash
)
    : m_governanceProposalId(std::move(governanceProposalId)),
      m_proposalType(std::move(proposalType)),
      m_treasuryProposal(std::move(treasuryProposal)),
      m_submittedAtBlock(submittedAtBlock),
      m_submittedBy(std::move(submittedBy)),
      m_governancePolicyVersion(std::move(governancePolicyVersion)),
      m_summaryHash(std::move(summaryHash)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_governanceProposalId.empty()) {
        m_rejectionReason = "GovernanceProposalEnvelope: governanceProposalId must not be empty.";
        return;
    }
    if (m_proposalType.empty()) {
        m_rejectionReason = "GovernanceProposalEnvelope: proposalType must not be empty.";
        return;
    }
    if (!m_treasuryProposal.isValid()) {
        m_rejectionReason = "GovernanceProposalEnvelope: treasuryProposal is invalid: " +
                            m_treasuryProposal.rejectionReason();
        return;
    }
    if (m_submittedBy.empty()) {
        m_rejectionReason = "GovernanceProposalEnvelope: submittedBy must not be empty.";
        return;
    }
    if (m_governancePolicyVersion.empty()) {
        m_rejectionReason = "GovernanceProposalEnvelope: governancePolicyVersion must not be empty.";
        return;
    }
    if (m_summaryHash.empty()) {
        m_rejectionReason = "GovernanceProposalEnvelope: summaryHash must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& GovernanceProposalEnvelope::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceProposalEnvelope::proposalType() const { return m_proposalType; }
const TreasuryProposal& GovernanceProposalEnvelope::treasuryProposal() const {
    return m_treasuryProposal;
}
std::uint64_t GovernanceProposalEnvelope::submittedAtBlock() const { return m_submittedAtBlock; }
const std::string& GovernanceProposalEnvelope::submittedBy() const { return m_submittedBy; }
const std::string& GovernanceProposalEnvelope::governancePolicyVersion() const {
    return m_governancePolicyVersion;
}
const std::string& GovernanceProposalEnvelope::summaryHash() const { return m_summaryHash; }
bool GovernanceProposalEnvelope::isValid() const { return m_valid; }
const std::string& GovernanceProposalEnvelope::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceProposalEnvelope::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceProposalEnvelope{"
        << "governanceProposalId=" << m_governanceProposalId
        << ";proposalType=" << m_proposalType
        << ";submittedAtBlock=" << m_submittedAtBlock
        << ";submittedBy=" << m_submittedBy
        << ";governancePolicyVersion=" << m_governancePolicyVersion
        << ";summaryHash=" << m_summaryHash
        << ";treasuryProposal=" << m_treasuryProposal.serialize()
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
