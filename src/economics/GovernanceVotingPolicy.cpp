#include "economics/GovernanceVotingPolicy.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceVotingPolicy::GovernanceVotingPolicy()
    : m_quorumVotingPower(utils::Amount::fromRawUnits(0)),
      m_approvalThresholdBasisPoints(0),
      m_minimumVotingPower(utils::Amount::fromRawUnits(0)),
      m_allowAbstain(false),
      m_allowVoteReplacement(false) {}

GovernanceVotingPolicy::GovernanceVotingPolicy(
    std::string policyVersion,
    utils::Amount quorumVotingPower,
    std::uint32_t approvalThresholdBasisPoints,
    utils::Amount minimumVotingPower,
    bool allowAbstain,
    bool allowVoteReplacement
)
    : m_policyVersion(std::move(policyVersion)),
      m_quorumVotingPower(quorumVotingPower),
      m_approvalThresholdBasisPoints(approvalThresholdBasisPoints),
      m_minimumVotingPower(minimumVotingPower),
      m_allowAbstain(allowAbstain),
      m_allowVoteReplacement(allowVoteReplacement) {}

const std::string& GovernanceVotingPolicy::policyVersion() const {
    return m_policyVersion;
}

utils::Amount GovernanceVotingPolicy::quorumVotingPower() const {
    return m_quorumVotingPower;
}

std::uint32_t GovernanceVotingPolicy::approvalThresholdBasisPoints() const {
    return m_approvalThresholdBasisPoints;
}

utils::Amount GovernanceVotingPolicy::minimumVotingPower() const {
    return m_minimumVotingPower;
}

bool GovernanceVotingPolicy::allowAbstain() const {
    return m_allowAbstain;
}

bool GovernanceVotingPolicy::allowVoteReplacement() const {
    return m_allowVoteReplacement;
}

bool GovernanceVotingPolicy::isValid() const {
    return rejectionReason().empty();
}

std::string GovernanceVotingPolicy::rejectionReason() const {
    if (m_policyVersion.empty()) {
        return "GovernanceVotingPolicy rejected: policyVersion is empty.";
    }
    if (m_quorumVotingPower.isNegative()) {
        return "GovernanceVotingPolicy rejected: quorumVotingPower is negative.";
    }
    if (m_approvalThresholdBasisPoints > 10000) {
        return "GovernanceVotingPolicy rejected: approvalThresholdBasisPoints exceeds 10000.";
    }
    if (m_minimumVotingPower.isNegative()) {
        return "GovernanceVotingPolicy rejected: minimumVotingPower is negative.";
    }
    return "";
}

std::string GovernanceVotingPolicy::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVotingPolicy{"
        << "policyVersion=" << m_policyVersion
        << ";quorumVotingPowerRaw=" << m_quorumVotingPower.rawUnits()
        << ";approvalThresholdBasisPoints=" << m_approvalThresholdBasisPoints
        << ";minimumVotingPowerRaw=" << m_minimumVotingPower.rawUnits()
        << ";allowAbstain=" << (m_allowAbstain ? "1" : "0")
        << ";allowVoteReplacement=" << (m_allowVoteReplacement ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
