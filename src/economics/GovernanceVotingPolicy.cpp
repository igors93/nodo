#include "economics/GovernanceVotingPolicy.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceVotingPolicy::GovernanceVotingPolicy()
    : m_quorumThresholdPower(0),
      m_approvalThresholdPower(0),
      m_allowAbstain(false),
      m_requireVoteProof(false),
      m_valid(false),
      m_rejectionReason("GovernanceVotingPolicy: default-constructed.") {}

GovernanceVotingPolicy::GovernanceVotingPolicy(
    std::string policyVersion,
    std::uint64_t quorumThresholdPower,
    std::uint64_t approvalThresholdPower,
    bool allowAbstain,
    bool requireVoteProof
)
    : m_policyVersion(std::move(policyVersion)),
      m_quorumThresholdPower(quorumThresholdPower),
      m_approvalThresholdPower(approvalThresholdPower),
      m_allowAbstain(allowAbstain),
      m_requireVoteProof(requireVoteProof),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_policyVersion.empty()) {
        m_rejectionReason = "GovernanceVotingPolicy: policyVersion must not be empty.";
        return;
    }

    if (m_quorumThresholdPower == 0) {
        m_rejectionReason =
            "GovernanceVotingPolicy: quorumThresholdPower must be positive.";
        return;
    }

    if (m_approvalThresholdPower == 0) {
        m_rejectionReason =
            "GovernanceVotingPolicy: approvalThresholdPower must be positive.";
        return;
    }

    m_valid = true;
}

const std::string& GovernanceVotingPolicy::policyVersion() const {
    return m_policyVersion;
}

std::uint64_t GovernanceVotingPolicy::quorumThresholdPower() const {
    return m_quorumThresholdPower;
}

std::uint64_t GovernanceVotingPolicy::approvalThresholdPower() const {
    return m_approvalThresholdPower;
}

bool GovernanceVotingPolicy::allowAbstain() const { return m_allowAbstain; }
bool GovernanceVotingPolicy::requireVoteProof() const { return m_requireVoteProof; }
bool GovernanceVotingPolicy::isValid() const { return m_valid; }
const std::string& GovernanceVotingPolicy::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceVotingPolicy::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVotingPolicy{"
        << "policyVersion=" << m_policyVersion
        << ";quorumThresholdPower=" << m_quorumThresholdPower
        << ";approvalThresholdPower=" << m_approvalThresholdPower
        << ";allowAbstain=" << (m_allowAbstain ? "1" : "0")
        << ";requireVoteProof=" << (m_requireVoteProof ? "1" : "0")
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
