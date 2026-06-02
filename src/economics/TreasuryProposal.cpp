#include "economics/TreasuryProposal.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

TreasuryProposal::TreasuryProposal()
    : m_amount(utils::Amount::fromRawUnits(0)),
      m_createdAtBlock(0),
      m_requestedEpoch(0),
      m_valid(false),
      m_rejectionReason("TreasuryProposal: default-constructed.") {}

TreasuryProposal::TreasuryProposal(
    std::string proposalId,
    std::string recipientAddress,
    utils::Amount amount,
    std::string purpose,
    std::uint64_t createdAtBlock,
    std::uint64_t requestedEpoch,
    std::string proposer
)
    : m_proposalId(std::move(proposalId)),
      m_recipientAddress(std::move(recipientAddress)),
      m_amount(amount),
      m_purpose(std::move(purpose)),
      m_createdAtBlock(createdAtBlock),
      m_requestedEpoch(requestedEpoch),
      m_proposer(std::move(proposer)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_proposalId.empty()) {
        m_rejectionReason = "TreasuryProposal: proposalId must not be empty.";
        return;
    }
    if (m_recipientAddress.empty()) {
        m_rejectionReason = "TreasuryProposal: recipientAddress must not be empty.";
        return;
    }
    if (!m_amount.isPositive()) {
        m_rejectionReason = "TreasuryProposal: amount must be positive.";
        return;
    }
    if (m_purpose.empty()) {
        m_rejectionReason = "TreasuryProposal: purpose must not be empty.";
        return;
    }
    if (m_proposer.empty()) {
        m_rejectionReason = "TreasuryProposal: proposer must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& TreasuryProposal::proposalId() const { return m_proposalId; }
const std::string& TreasuryProposal::recipientAddress() const { return m_recipientAddress; }
utils::Amount TreasuryProposal::amount() const { return m_amount; }
const std::string& TreasuryProposal::purpose() const { return m_purpose; }
std::uint64_t TreasuryProposal::createdAtBlock() const { return m_createdAtBlock; }
std::uint64_t TreasuryProposal::requestedEpoch() const { return m_requestedEpoch; }
const std::string& TreasuryProposal::proposer() const { return m_proposer; }
bool TreasuryProposal::isValid() const { return m_valid; }
const std::string& TreasuryProposal::rejectionReason() const { return m_rejectionReason; }

std::string TreasuryProposal::serialize() const {
    std::ostringstream oss;
    oss << "TreasuryProposal{"
        << "id=" << m_proposalId
        << ";recipient=" << m_recipientAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";purpose=" << m_purpose
        << ";createdAtBlock=" << m_createdAtBlock
        << ";epoch=" << m_requestedEpoch
        << ";proposer=" << m_proposer
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
