#include "economics/GovernanceVoteRecord.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice) {
    switch (choice) {
        case GovernanceVoteChoice::YES:
            return "YES";
        case GovernanceVoteChoice::NO:
            return "NO";
        case GovernanceVoteChoice::ABSTAIN:
            return "ABSTAIN";
        default:
            return "UNKNOWN";
    }
}

bool governanceVoteChoiceFromString(
    const std::string& value,
    GovernanceVoteChoice& out
) {
    if (value == "YES") {
        out = GovernanceVoteChoice::YES;
        return true;
    }
    if (value == "NO") {
        out = GovernanceVoteChoice::NO;
        return true;
    }
    if (value == "ABSTAIN") {
        out = GovernanceVoteChoice::ABSTAIN;
        return true;
    }
    return false;
}

GovernanceVoteRecord::GovernanceVoteRecord()
    : m_voteChoice(GovernanceVoteChoice::NO),
      m_votingPower(utils::Amount::fromRawUnits(0)),
      m_castAtBlock(0) {}

GovernanceVoteRecord::GovernanceVoteRecord(
    std::string voteId,
    std::string governanceProposalId,
    std::string voterId,
    GovernanceVoteChoice voteChoice,
    utils::Amount votingPower,
    std::uint64_t castAtBlock,
    std::string votingPowerSource,
    std::string voteProof,
    std::string policyVersion
)
    : m_voteId(std::move(voteId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_voterId(std::move(voterId)),
      m_voteChoice(voteChoice),
      m_votingPower(votingPower),
      m_castAtBlock(castAtBlock),
      m_votingPowerSource(std::move(votingPowerSource)),
      m_voteProof(std::move(voteProof)),
      m_policyVersion(std::move(policyVersion)) {}

const std::string& GovernanceVoteRecord::voteId() const { return m_voteId; }
const std::string& GovernanceVoteRecord::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceVoteRecord::voterId() const { return m_voterId; }
GovernanceVoteChoice GovernanceVoteRecord::voteChoice() const { return m_voteChoice; }
utils::Amount GovernanceVoteRecord::votingPower() const { return m_votingPower; }
std::uint64_t GovernanceVoteRecord::castAtBlock() const { return m_castAtBlock; }
const std::string& GovernanceVoteRecord::votingPowerSource() const {
    return m_votingPowerSource;
}
const std::string& GovernanceVoteRecord::voteProof() const { return m_voteProof; }
const std::string& GovernanceVoteRecord::policyVersion() const {
    return m_policyVersion;
}

bool GovernanceVoteRecord::isValid() const {
    return rejectionReason().empty();
}

std::string GovernanceVoteRecord::rejectionReason() const {
    if (m_voteId.empty()) {
        return "GovernanceVoteRecord rejected: voteId is empty.";
    }
    if (m_governanceProposalId.empty()) {
        return "GovernanceVoteRecord rejected: governanceProposalId is empty.";
    }
    if (m_voterId.empty()) {
        return "GovernanceVoteRecord rejected: voterId is empty.";
    }
    if (m_castAtBlock == 0) {
        return "GovernanceVoteRecord rejected: castAtBlock must be positive.";
    }
    if (m_votingPowerSource.empty()) {
        return "GovernanceVoteRecord rejected: votingPowerSource is empty.";
    }
    if (m_voteProof.empty()) {
        return "GovernanceVoteRecord rejected: voteProof is empty.";
    }
    if (m_policyVersion.empty()) {
        return "GovernanceVoteRecord rejected: policyVersion is empty.";
    }
    if (m_votingPower.isNegative()) {
        return "GovernanceVoteRecord rejected: votingPower is negative.";
    }
    return "";
}

bool GovernanceVoteRecord::isValidUnderPolicy(
    const GovernanceVotingPolicy& policy
) const {
    return policyRejectionReason(policy).empty();
}

std::string GovernanceVoteRecord::policyRejectionReason(
    const GovernanceVotingPolicy& policy
) const {
    if (!isValid()) {
        return rejectionReason();
    }
    if (!policy.isValid()) {
        return "GovernanceVoteRecord policy check: policy is invalid: " +
               policy.rejectionReason();
    }
    if (m_policyVersion != policy.policyVersion()) {
        return "GovernanceVoteRecord policy check: policyVersion='" +
               m_policyVersion + "' does not match policy.policyVersion='" +
               policy.policyVersion() + "'.";
    }
    if (m_votingPower < policy.minimumVotingPower()) {
        return "GovernanceVoteRecord policy check: votingPower=" +
               std::to_string(m_votingPower.rawUnits()) +
               " is below minimumVotingPower=" +
               std::to_string(policy.minimumVotingPower().rawUnits()) + ".";
    }
    if (m_voteChoice == GovernanceVoteChoice::ABSTAIN &&
        !policy.allowAbstain()) {
        return "GovernanceVoteRecord policy check: ABSTAIN is not allowed.";
    }
    return "";
}

std::string GovernanceVoteRecord::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVoteRecord{"
        << "voteId=" << m_voteId
        << ";governanceProposalId=" << m_governanceProposalId
        << ";voterId=" << m_voterId
        << ";choice=" << governanceVoteChoiceToString(m_voteChoice)
        << ";votingPowerRaw=" << m_votingPower.rawUnits()
        << ";castAtBlock=" << m_castAtBlock
        << ";votingPowerSource=" << m_votingPowerSource
        << ";voteProof=" << m_voteProof
        << ";policyVersion=" << m_policyVersion
        << "}";
    return oss.str();
}

} // namespace nodo::economics
