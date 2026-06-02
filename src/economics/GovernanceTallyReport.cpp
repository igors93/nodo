#include "economics/GovernanceTallyReport.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

namespace {

std::string boolText(bool value) {
    return value ? "1" : "0";
}

} // namespace

GovernanceTallyReport::GovernanceTallyReport()
    : m_totalVotingPower(0),
      m_yesVotingPower(0),
      m_noVotingPower(0),
      m_abstainVotingPower(0),
      m_yesVoteCount(0),
      m_noVoteCount(0),
      m_abstainVoteCount(0),
      m_quorumMet(false),
      m_approvalThresholdMet(false),
      m_approved(false),
      m_valid(false),
      m_rejectionReason("GovernanceTallyReport: default-constructed.") {}

GovernanceTallyReport::GovernanceTallyReport(
    std::string governanceProposalId,
    std::string policyVersion,
    std::uint64_t totalVotingPower,
    std::uint64_t yesVotingPower,
    std::uint64_t noVotingPower,
    std::uint64_t abstainVotingPower,
    std::uint64_t yesVoteCount,
    std::uint64_t noVoteCount,
    std::uint64_t abstainVoteCount,
    bool quorumMet,
    bool approvalThresholdMet,
    bool approved,
    std::string tallyProof
)
    : m_governanceProposalId(std::move(governanceProposalId)),
      m_policyVersion(std::move(policyVersion)),
      m_totalVotingPower(totalVotingPower),
      m_yesVotingPower(yesVotingPower),
      m_noVotingPower(noVotingPower),
      m_abstainVotingPower(abstainVotingPower),
      m_yesVoteCount(yesVoteCount),
      m_noVoteCount(noVoteCount),
      m_abstainVoteCount(abstainVoteCount),
      m_quorumMet(quorumMet),
      m_approvalThresholdMet(approvalThresholdMet),
      m_approved(approved),
      m_tallyProof(std::move(tallyProof)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_governanceProposalId.empty()) {
        m_rejectionReason =
            "GovernanceTallyReport: governanceProposalId must not be empty.";
        return;
    }

    if (m_policyVersion.empty()) {
        m_rejectionReason = "GovernanceTallyReport: policyVersion must not be empty.";
        return;
    }

    if (m_totalVotingPower == 0) {
        m_rejectionReason =
            "GovernanceTallyReport: totalVotingPower must be positive.";
        return;
    }

    if (m_totalVotingPower !=
        m_yesVotingPower + m_noVotingPower + m_abstainVotingPower) {
        m_rejectionReason =
            "GovernanceTallyReport: totalVotingPower does not match choice totals.";
        return;
    }

    if (m_approved != (m_quorumMet && m_approvalThresholdMet)) {
        m_rejectionReason =
            "GovernanceTallyReport: approved must equal quorumMet && "
            "approvalThresholdMet.";
        return;
    }

    if (m_tallyProof.empty()) {
        m_rejectionReason = "GovernanceTallyReport: tallyProof must not be empty.";
        return;
    }

    const std::string expectedProof = buildTallyProof(
        m_governanceProposalId,
        m_policyVersion,
        m_totalVotingPower,
        m_yesVotingPower,
        m_noVotingPower,
        m_abstainVotingPower,
        m_yesVoteCount,
        m_noVoteCount,
        m_abstainVoteCount,
        m_quorumMet,
        m_approvalThresholdMet,
        m_approved
    );

    if (m_tallyProof != expectedProof) {
        m_rejectionReason =
            "GovernanceTallyReport: tallyProof does not match tally fields.";
        return;
    }

    m_valid = true;
}

const std::string& GovernanceTallyReport::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceTallyReport::policyVersion() const {
    return m_policyVersion;
}
std::uint64_t GovernanceTallyReport::totalVotingPower() const {
    return m_totalVotingPower;
}
std::uint64_t GovernanceTallyReport::yesVotingPower() const {
    return m_yesVotingPower;
}
std::uint64_t GovernanceTallyReport::noVotingPower() const {
    return m_noVotingPower;
}
std::uint64_t GovernanceTallyReport::abstainVotingPower() const {
    return m_abstainVotingPower;
}
std::uint64_t GovernanceTallyReport::yesVoteCount() const { return m_yesVoteCount; }
std::uint64_t GovernanceTallyReport::noVoteCount() const { return m_noVoteCount; }
std::uint64_t GovernanceTallyReport::abstainVoteCount() const {
    return m_abstainVoteCount;
}
bool GovernanceTallyReport::quorumMet() const { return m_quorumMet; }
bool GovernanceTallyReport::approvalThresholdMet() const {
    return m_approvalThresholdMet;
}
bool GovernanceTallyReport::approved() const { return m_approved; }
const std::string& GovernanceTallyReport::tallyProof() const {
    return m_tallyProof;
}
bool GovernanceTallyReport::isValid() const { return m_valid; }
const std::string& GovernanceTallyReport::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceTallyReport::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceTallyReport{"
        << "governanceProposalId=" << m_governanceProposalId
        << ";policyVersion=" << m_policyVersion
        << ";totalVotingPower=" << m_totalVotingPower
        << ";yesVotingPower=" << m_yesVotingPower
        << ";noVotingPower=" << m_noVotingPower
        << ";abstainVotingPower=" << m_abstainVotingPower
        << ";yesVoteCount=" << m_yesVoteCount
        << ";noVoteCount=" << m_noVoteCount
        << ";abstainVoteCount=" << m_abstainVoteCount
        << ";quorumMet=" << boolText(m_quorumMet)
        << ";approvalThresholdMet=" << boolText(m_approvalThresholdMet)
        << ";approved=" << boolText(m_approved)
        << ";tallyProof=" << m_tallyProof
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

std::string GovernanceTallyReport::buildTallyProof(
    const std::string& governanceProposalId,
    const std::string& policyVersion,
    std::uint64_t totalVotingPower,
    std::uint64_t yesVotingPower,
    std::uint64_t noVotingPower,
    std::uint64_t abstainVotingPower,
    std::uint64_t yesVoteCount,
    std::uint64_t noVoteCount,
    std::uint64_t abstainVoteCount,
    bool quorumMet,
    bool approvalThresholdMet,
    bool approved
) {
    return "governance-tally:"
        + governanceProposalId + ":"
        + policyVersion + ":"
        + std::to_string(totalVotingPower) + ":"
        + std::to_string(yesVotingPower) + ":"
        + std::to_string(noVotingPower) + ":"
        + std::to_string(abstainVotingPower) + ":"
        + std::to_string(yesVoteCount) + ":"
        + std::to_string(noVoteCount) + ":"
        + std::to_string(abstainVoteCount) + ":"
        + boolText(quorumMet) + ":"
        + boolText(approvalThresholdMet) + ":"
        + boolText(approved);
}

} // namespace nodo::economics
