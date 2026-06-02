#include "economics/TreasuryApproval.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

TreasuryApproval::TreasuryApproval()
    : m_approvedAtBlock(0),
      m_valid(false),
      m_rejectionReason("TreasuryApproval: default-constructed.") {}

TreasuryApproval::TreasuryApproval(
    std::string approvalId,
    std::string proposalId,
    std::uint64_t approvedAtBlock,
    std::string approver,
    std::string approvalProof
)
    : m_approvalId(std::move(approvalId)),
      m_proposalId(std::move(proposalId)),
      m_approvedAtBlock(approvedAtBlock),
      m_approver(std::move(approver)),
      m_approvalProof(std::move(approvalProof)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_approvalId.empty()) {
        m_rejectionReason = "TreasuryApproval: approvalId must not be empty.";
        return;
    }
    if (m_proposalId.empty()) {
        m_rejectionReason = "TreasuryApproval: proposalId must not be empty.";
        return;
    }
    if (m_approver.empty()) {
        m_rejectionReason = "TreasuryApproval: approver must not be empty.";
        return;
    }
    if (m_approvalProof.empty()) {
        m_rejectionReason = "TreasuryApproval: approvalProof must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& TreasuryApproval::approvalId() const { return m_approvalId; }
const std::string& TreasuryApproval::proposalId() const { return m_proposalId; }
std::uint64_t TreasuryApproval::approvedAtBlock() const { return m_approvedAtBlock; }
const std::string& TreasuryApproval::approver() const { return m_approver; }
const std::string& TreasuryApproval::approvalProof() const { return m_approvalProof; }
bool TreasuryApproval::isValid() const { return m_valid; }
const std::string& TreasuryApproval::rejectionReason() const { return m_rejectionReason; }

std::string TreasuryApproval::serialize() const {
    std::ostringstream oss;
    oss << "TreasuryApproval{"
        << "id=" << m_approvalId
        << ";proposalId=" << m_proposalId
        << ";approvedAtBlock=" << m_approvedAtBlock
        << ";approver=" << m_approver
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
