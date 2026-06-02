#include "economics/TreasuryPolicy.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

TreasuryPolicy::TreasuryPolicy()
    : m_maxSpendPerEpoch(utils::Amount::fromRawUnits(0)),
      m_maxSpendPerProposal(utils::Amount::fromRawUnits(0)),
      m_timelockBlocks(0),
      m_requireApproval(true),
      m_allowSpendingWhenLocked(false),
      m_valid(false),
      m_rejectionReason("TreasuryPolicy: default-constructed.") {}

TreasuryPolicy::TreasuryPolicy(
    std::string policyVersion,
    utils::Amount maxSpendPerEpoch,
    utils::Amount maxSpendPerProposal,
    std::uint64_t timelockBlocks,
    bool requireApproval,
    bool allowSpendingWhenLocked
)
    : m_policyVersion(std::move(policyVersion)),
      m_maxSpendPerEpoch(maxSpendPerEpoch),
      m_maxSpendPerProposal(maxSpendPerProposal),
      m_timelockBlocks(timelockBlocks),
      m_requireApproval(requireApproval),
      m_allowSpendingWhenLocked(allowSpendingWhenLocked),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_policyVersion.empty()) {
        m_rejectionReason = "TreasuryPolicy: policyVersion must not be empty.";
        return;
    }
    if (m_maxSpendPerEpoch.isNegative()) {
        m_rejectionReason = "TreasuryPolicy: maxSpendPerEpoch must not be negative.";
        return;
    }
    if (m_maxSpendPerProposal.isNegative()) {
        m_rejectionReason = "TreasuryPolicy: maxSpendPerProposal must not be negative.";
        return;
    }
    m_valid = true;
}

const std::string& TreasuryPolicy::policyVersion() const { return m_policyVersion; }
utils::Amount TreasuryPolicy::maxSpendPerEpoch() const { return m_maxSpendPerEpoch; }
utils::Amount TreasuryPolicy::maxSpendPerProposal() const { return m_maxSpendPerProposal; }
std::uint64_t TreasuryPolicy::timelockBlocks() const { return m_timelockBlocks; }
bool TreasuryPolicy::requireApproval() const { return m_requireApproval; }
bool TreasuryPolicy::allowSpendingWhenLocked() const { return m_allowSpendingWhenLocked; }
bool TreasuryPolicy::isValid() const { return m_valid; }
const std::string& TreasuryPolicy::rejectionReason() const { return m_rejectionReason; }

std::string TreasuryPolicy::serialize() const {
    std::ostringstream oss;
    oss << "TreasuryPolicy{"
        << "policyVersion=" << m_policyVersion
        << ";maxSpendPerEpochRaw=" << m_maxSpendPerEpoch.rawUnits()
        << ";maxSpendPerProposalRaw=" << m_maxSpendPerProposal.rawUnits()
        << ";timelockBlocks=" << m_timelockBlocks
        << ";requireApproval=" << (m_requireApproval ? "1" : "0")
        << ";allowSpendingWhenLocked=" << (m_allowSpendingWhenLocked ? "1" : "0")
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
