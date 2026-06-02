#include "economics/GovernanceTallyReport.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

namespace {

std::string boolText(bool value) {
    return value ? "1" : "0";
}

bool checkedAddRaw(
    std::uint64_t& target,
    std::int64_t raw,
    std::string& reason
) {
    if (raw < 0) {
        reason = "GovernanceTallyReport: negative voting power.";
        return false;
    }
    const std::uint64_t add = static_cast<std::uint64_t>(raw);
    if (target > std::numeric_limits<std::uint64_t>::max() - add) {
        reason = "GovernanceTallyReport: voting power overflow.";
        return false;
    }
    target += add;
    return true;
}

bool checkedAddU64(
    std::uint64_t& target,
    std::uint64_t add,
    std::string& reason
) {
    if (target > std::numeric_limits<std::uint64_t>::max() - add) {
        reason = "GovernanceTallyReport: voting power overflow.";
        return false;
    }
    target += add;
    return true;
}

utils::Amount amountFromU64(std::uint64_t raw) {
    if (raw > static_cast<std::uint64_t>(
                  std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("GovernanceTallyReport: amount exceeds int64.");
    }
    return utils::Amount::fromRawUnits(static_cast<std::int64_t>(raw));
}

} // namespace

GovernanceTallyReport::GovernanceTallyReport()
    : m_totalVotingPower(utils::Amount::fromRawUnits(0)),
      m_yesVotingPower(utils::Amount::fromRawUnits(0)),
      m_noVotingPower(utils::Amount::fromRawUnits(0)),
      m_abstainVotingPower(utils::Amount::fromRawUnits(0)),
      m_voteCount(0),
      m_yesCount(0),
      m_noCount(0),
      m_abstainCount(0),
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
    std::uint64_t yesCount,
    std::uint64_t noCount,
    std::uint64_t abstainCount,
    bool quorumMet,
    bool approvalThresholdMet,
    bool approved,
    std::string tallyProof
)
    : m_governanceProposalId(std::move(governanceProposalId)),
      m_policyVersion(std::move(policyVersion)),
      m_totalVotingPower(utils::Amount::fromRawUnits(0)),
      m_yesVotingPower(utils::Amount::fromRawUnits(0)),
      m_noVotingPower(utils::Amount::fromRawUnits(0)),
      m_abstainVotingPower(utils::Amount::fromRawUnits(0)),
      m_voteCount(0),
      m_yesCount(static_cast<std::size_t>(yesCount)),
      m_noCount(static_cast<std::size_t>(noCount)),
      m_abstainCount(static_cast<std::size_t>(abstainCount)),
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
    if (m_tallyProof.empty()) {
        m_rejectionReason = "GovernanceTallyReport: tallyProof must not be empty.";
        return;
    }

    try {
        m_totalVotingPower = amountFromU64(totalVotingPower);
        m_yesVotingPower = amountFromU64(yesVotingPower);
        m_noVotingPower = amountFromU64(noVotingPower);
        m_abstainVotingPower = amountFromU64(abstainVotingPower);
    } catch (const std::exception& error) {
        m_rejectionReason = error.what();
        return;
    }

    std::uint64_t rebuiltTotal = 0;
    std::string sumReason;
    if (!checkedAddRaw(
            rebuiltTotal,
            static_cast<std::int64_t>(yesVotingPower),
            sumReason) ||
        !checkedAddRaw(
            rebuiltTotal,
            static_cast<std::int64_t>(noVotingPower),
            sumReason) ||
        !checkedAddRaw(
            rebuiltTotal,
            static_cast<std::int64_t>(abstainVotingPower),
            sumReason)) {
        m_rejectionReason = sumReason;
        return;
    }

    if (rebuiltTotal != totalVotingPower) {
        m_rejectionReason =
            "GovernanceTallyReport: totalVotingPower does not match choice totals.";
        return;
    }

    if (yesCount > std::numeric_limits<std::size_t>::max() ||
        noCount > std::numeric_limits<std::size_t>::max() ||
        abstainCount > std::numeric_limits<std::size_t>::max()) {
        m_rejectionReason = "GovernanceTallyReport: vote count exceeds size_t.";
        return;
    }

    m_voteCount = m_yesCount + m_noCount + m_abstainCount;
    if (m_voteCount < m_yesCount || m_voteCount < m_noCount) {
        m_rejectionReason = "GovernanceTallyReport: vote count overflow.";
        return;
    }

    if (m_approved != (m_quorumMet && m_approvalThresholdMet)) {
        m_rejectionReason =
            "GovernanceTallyReport: approved must equal quorumMet && approvalThresholdMet.";
        return;
    }

    const std::string expectedProof = buildTallyProof(
        m_governanceProposalId,
        m_policyVersion,
        totalVotingPower,
        yesVotingPower,
        noVotingPower,
        abstainVotingPower,
        yesCount,
        noCount,
        abstainCount,
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

GovernanceTallyReport GovernanceTallyReport::build(
    const GovernanceVoteSetAuditResult& auditResult,
    const GovernanceVotingPolicy& votingPolicy
) {
    if (!auditResult.accepted()) {
        GovernanceTallyReport report;
        report.m_rejectionReason =
            "GovernanceTallyReport: vote set audit failed: " +
            auditResult.reason();
        return report;
    }

    if (!votingPolicy.isValid()) {
        GovernanceTallyReport report;
        report.m_rejectionReason =
            "GovernanceTallyReport: voting policy is invalid: " +
            votingPolicy.rejectionReason();
        return report;
    }

    if (auditResult.canonicalVotes().empty()) {
        GovernanceTallyReport report;
        report.m_rejectionReason =
            "GovernanceTallyReport: no canonical votes to tally.";
        return report;
    }

    const std::string governanceProposalId =
        auditResult.canonicalVotes().front()
            .voteRecord()
            .governanceProposalId();
    std::uint64_t yesPower = 0;
    std::uint64_t noPower = 0;
    std::uint64_t abstainPower = 0;
    std::uint64_t yesCount = 0;
    std::uint64_t noCount = 0;
    std::uint64_t abstainCount = 0;

    for (const GovernanceVoteEvidence& evidence : auditResult.canonicalVotes()) {
        const GovernanceVoteRecord& vote = evidence.voteRecord();
        std::string addReason;
        switch (vote.voteChoice()) {
            case GovernanceVoteChoice::YES:
                if (!checkedAddRaw(yesPower, vote.votingPower().rawUnits(), addReason)) {
                    GovernanceTallyReport report;
                    report.m_rejectionReason = addReason;
                    return report;
                }
                ++yesCount;
                break;
            case GovernanceVoteChoice::NO:
                if (!checkedAddRaw(noPower, vote.votingPower().rawUnits(), addReason)) {
                    GovernanceTallyReport report;
                    report.m_rejectionReason = addReason;
                    return report;
                }
                ++noCount;
                break;
            case GovernanceVoteChoice::ABSTAIN:
                if (!checkedAddRaw(
                        abstainPower,
                        vote.votingPower().rawUnits(),
                        addReason)) {
                    GovernanceTallyReport report;
                    report.m_rejectionReason = addReason;
                    return report;
                }
                ++abstainCount;
                break;
        }
    }

    std::uint64_t totalPower = 0;
    std::string addReason;
    if (!checkedAddU64(totalPower, yesPower, addReason) ||
        !checkedAddU64(totalPower, noPower, addReason) ||
        !checkedAddU64(totalPower, abstainPower, addReason)) {
        GovernanceTallyReport report;
        report.m_rejectionReason = addReason;
        return report;
    }

    const bool quorumMet =
        totalPower >=
        static_cast<std::uint64_t>(votingPolicy.quorumVotingPower().rawUnits());

    bool approvalThresholdMet = false;
    std::uint64_t yesNoPower = 0;
    if (!checkedAddU64(yesNoPower, yesPower, addReason) ||
        !checkedAddU64(yesNoPower, noPower, addReason)) {
        GovernanceTallyReport report;
        report.m_rejectionReason = addReason;
        return report;
    }
    if (yesNoPower != 0) {
        const unsigned __int128 lhs =
            static_cast<unsigned __int128>(yesPower) * 10000U;
        const unsigned __int128 rhs =
            static_cast<unsigned __int128>(yesNoPower) *
            votingPolicy.approvalThresholdBasisPoints();
        approvalThresholdMet = lhs >= rhs;
    }

    const bool approved = quorumMet && approvalThresholdMet;
    const std::string proof = buildTallyProof(
        governanceProposalId,
        votingPolicy.policyVersion(),
        totalPower,
        yesPower,
        noPower,
        abstainPower,
        yesCount,
        noCount,
        abstainCount,
        quorumMet,
        approvalThresholdMet,
        approved
    );

    return GovernanceTallyReport(
        governanceProposalId,
        votingPolicy.policyVersion(),
        totalPower,
        yesPower,
        noPower,
        abstainPower,
        yesCount,
        noCount,
        abstainCount,
        quorumMet,
        approvalThresholdMet,
        approved,
        proof
    );
}

const std::string& GovernanceTallyReport::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceTallyReport::policyVersion() const {
    return m_policyVersion;
}
utils::Amount GovernanceTallyReport::totalVotingPower() const {
    return m_totalVotingPower;
}
utils::Amount GovernanceTallyReport::yesVotingPower() const {
    return m_yesVotingPower;
}
utils::Amount GovernanceTallyReport::noVotingPower() const {
    return m_noVotingPower;
}
utils::Amount GovernanceTallyReport::abstainVotingPower() const {
    return m_abstainVotingPower;
}
std::size_t GovernanceTallyReport::voteCount() const { return m_voteCount; }
std::size_t GovernanceTallyReport::yesCount() const { return m_yesCount; }
std::size_t GovernanceTallyReport::noCount() const { return m_noCount; }
std::size_t GovernanceTallyReport::abstainCount() const { return m_abstainCount; }
std::size_t GovernanceTallyReport::yesVoteCount() const { return m_yesCount; }
std::size_t GovernanceTallyReport::noVoteCount() const { return m_noCount; }
std::size_t GovernanceTallyReport::abstainVoteCount() const {
    return m_abstainCount;
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
        << ";totalVotingPowerRaw=" << m_totalVotingPower.rawUnits()
        << ";yesVotingPowerRaw=" << m_yesVotingPower.rawUnits()
        << ";noVotingPowerRaw=" << m_noVotingPower.rawUnits()
        << ";abstainVotingPowerRaw=" << m_abstainVotingPower.rawUnits()
        << ";voteCount=" << m_voteCount
        << ";yesCount=" << m_yesCount
        << ";noCount=" << m_noCount
        << ";abstainCount=" << m_abstainCount
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
    std::uint64_t yesCount,
    std::uint64_t noCount,
    std::uint64_t abstainCount,
    bool quorumMet,
    bool approvalThresholdMet,
    bool approved
) {
    return "governance-tally-v1:" +
           governanceProposalId + ":" +
           policyVersion + ":" +
           std::to_string(totalVotingPower) + ":" +
           std::to_string(yesVotingPower) + ":" +
           std::to_string(noVotingPower) + ":" +
           std::to_string(abstainVotingPower) + ":" +
           std::to_string(yesCount) + ":" +
           std::to_string(noCount) + ":" +
           std::to_string(abstainCount) + ":" +
           boolText(quorumMet) + ":" +
           boolText(approvalThresholdMet) + ":" +
           boolText(approved);
}

} // namespace nodo::economics
