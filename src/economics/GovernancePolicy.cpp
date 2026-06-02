#include "economics/GovernancePolicy.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernancePolicy::GovernancePolicy()
    : m_reviewPeriodBlocks(0),
      m_decisionTimelockBlocks(0),
      m_requireDecisionProof(false),
      m_allowEmergencyApproval(false),
      m_valid(false),
      m_rejectionReason("GovernancePolicy: default-constructed.") {}

GovernancePolicy::GovernancePolicy(
    std::string policyVersion,
    std::uint64_t reviewPeriodBlocks,
    std::uint64_t decisionTimelockBlocks,
    bool requireDecisionProof,
    bool allowEmergencyApproval
)
    : m_policyVersion(std::move(policyVersion)),
      m_reviewPeriodBlocks(reviewPeriodBlocks),
      m_decisionTimelockBlocks(decisionTimelockBlocks),
      m_requireDecisionProof(requireDecisionProof),
      m_allowEmergencyApproval(allowEmergencyApproval),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_policyVersion.empty()) {
        m_rejectionReason = "GovernancePolicy: policyVersion must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& GovernancePolicy::policyVersion() const { return m_policyVersion; }
std::uint64_t GovernancePolicy::reviewPeriodBlocks() const { return m_reviewPeriodBlocks; }
std::uint64_t GovernancePolicy::decisionTimelockBlocks() const { return m_decisionTimelockBlocks; }
bool GovernancePolicy::requireDecisionProof() const { return m_requireDecisionProof; }
bool GovernancePolicy::allowEmergencyApproval() const { return m_allowEmergencyApproval; }
bool GovernancePolicy::isValid() const { return m_valid; }
const std::string& GovernancePolicy::rejectionReason() const { return m_rejectionReason; }

std::string GovernancePolicy::serialize() const {
    std::ostringstream oss;
    oss << "GovernancePolicy{"
        << "policyVersion=" << m_policyVersion
        << ";reviewPeriodBlocks=" << m_reviewPeriodBlocks
        << ";decisionTimelockBlocks=" << m_decisionTimelockBlocks
        << ";requireDecisionProof=" << (m_requireDecisionProof ? "1" : "0")
        << ";allowEmergencyApproval=" << (m_allowEmergencyApproval ? "1" : "0")
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
