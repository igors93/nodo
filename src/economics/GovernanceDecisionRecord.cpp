#include "economics/GovernanceDecisionRecord.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

std::string governanceDecisionStatusToString(GovernanceDecisionStatus status) {
    switch (status) {
        case GovernanceDecisionStatus::APPROVED:  return "APPROVED";
        case GovernanceDecisionStatus::REJECTED:  return "REJECTED";
        case GovernanceDecisionStatus::EXPIRED:   return "EXPIRED";
        case GovernanceDecisionStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

bool governanceDecisionStatusFromString(
    const std::string& s,
    GovernanceDecisionStatus& out
) {
    if (s == "APPROVED")  { out = GovernanceDecisionStatus::APPROVED;  return true; }
    if (s == "REJECTED")  { out = GovernanceDecisionStatus::REJECTED;  return true; }
    if (s == "EXPIRED")   { out = GovernanceDecisionStatus::EXPIRED;   return true; }
    if (s == "CANCELLED") { out = GovernanceDecisionStatus::CANCELLED; return true; }
    return false;
}

GovernanceDecisionRecord::GovernanceDecisionRecord()
    : m_decisionStatus(GovernanceDecisionStatus::REJECTED),
      m_decidedAtBlock(0),
      m_valid(false),
      m_rejectionReason("GovernanceDecisionRecord: default-constructed.") {}

GovernanceDecisionRecord::GovernanceDecisionRecord(
    std::string decisionId,
    std::string governanceProposalId,
    std::string proposalType,
    GovernanceDecisionStatus decisionStatus,
    std::uint64_t decidedAtBlock,
    std::string decisionMaker,
    std::string decisionProof,
    std::string policyVersion
)
    : m_decisionId(std::move(decisionId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_proposalType(std::move(proposalType)),
      m_decisionStatus(decisionStatus),
      m_decidedAtBlock(decidedAtBlock),
      m_decisionMaker(std::move(decisionMaker)),
      m_decisionProof(std::move(decisionProof)),
      m_policyVersion(std::move(policyVersion)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_decisionId.empty()) {
        m_rejectionReason = "GovernanceDecisionRecord: decisionId must not be empty.";
        return;
    }
    if (m_governanceProposalId.empty()) {
        m_rejectionReason = "GovernanceDecisionRecord: governanceProposalId must not be empty.";
        return;
    }
    if (m_proposalType.empty()) {
        m_rejectionReason = "GovernanceDecisionRecord: proposalType must not be empty.";
        return;
    }
    if (m_decisionMaker.empty()) {
        m_rejectionReason = "GovernanceDecisionRecord: decisionMaker must not be empty.";
        return;
    }
    if (m_policyVersion.empty()) {
        m_rejectionReason = "GovernanceDecisionRecord: policyVersion must not be empty.";
        return;
    }
    // decisionProof emptiness check is policy-dependent; enforced by GovernanceApprovalBridge.
    m_valid = true;
}

const std::string& GovernanceDecisionRecord::decisionId() const { return m_decisionId; }
const std::string& GovernanceDecisionRecord::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceDecisionRecord::proposalType() const { return m_proposalType; }
GovernanceDecisionStatus GovernanceDecisionRecord::decisionStatus() const {
    return m_decisionStatus;
}
std::uint64_t GovernanceDecisionRecord::decidedAtBlock() const { return m_decidedAtBlock; }
const std::string& GovernanceDecisionRecord::decisionMaker() const { return m_decisionMaker; }
const std::string& GovernanceDecisionRecord::decisionProof() const { return m_decisionProof; }
const std::string& GovernanceDecisionRecord::policyVersion() const { return m_policyVersion; }

bool GovernanceDecisionRecord::approved() const {
    return m_decisionStatus == GovernanceDecisionStatus::APPROVED;
}

bool GovernanceDecisionRecord::isValid() const { return m_valid; }
const std::string& GovernanceDecisionRecord::rejectionReason() const { return m_rejectionReason; }

std::string GovernanceDecisionRecord::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceDecisionRecord{"
        << "decisionId=" << m_decisionId
        << ";governanceProposalId=" << m_governanceProposalId
        << ";proposalType=" << m_proposalType
        << ";decisionStatus=" << governanceDecisionStatusToString(m_decisionStatus)
        << ";decidedAtBlock=" << m_decidedAtBlock
        << ";decisionMaker=" << m_decisionMaker
        << ";policyVersion=" << m_policyVersion
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
