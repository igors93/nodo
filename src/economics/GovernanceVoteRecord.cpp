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
    : m_choice(GovernanceVoteChoice::ABSTAIN),
      m_votingPower(0),
      m_votedAtBlock(0),
      m_valid(false),
      m_rejectionReason("GovernanceVoteRecord: default-constructed.") {}

GovernanceVoteRecord::GovernanceVoteRecord(
    std::string voteId,
    std::string governanceProposalId,
    std::string voterId,
    GovernanceVoteChoice choice,
    std::uint64_t votingPower,
    std::uint64_t votedAtBlock,
    std::string policyVersion
)
    : m_voteId(std::move(voteId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_voterId(std::move(voterId)),
      m_choice(choice),
      m_votingPower(votingPower),
      m_votedAtBlock(votedAtBlock),
      m_policyVersion(std::move(policyVersion)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_voteId.empty()) {
        m_rejectionReason = "GovernanceVoteRecord: voteId must not be empty.";
        return;
    }
    if (m_governanceProposalId.empty()) {
        m_rejectionReason =
            "GovernanceVoteRecord: governanceProposalId must not be empty.";
        return;
    }
    if (m_voterId.empty()) {
        m_rejectionReason = "GovernanceVoteRecord: voterId must not be empty.";
        return;
    }
    if (m_votingPower == 0) {
        m_rejectionReason = "GovernanceVoteRecord: votingPower must be positive.";
        return;
    }
    if (m_votedAtBlock == 0) {
        m_rejectionReason = "GovernanceVoteRecord: votedAtBlock must be positive.";
        return;
    }
    if (m_policyVersion.empty()) {
        m_rejectionReason = "GovernanceVoteRecord: policyVersion must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& GovernanceVoteRecord::voteId() const { return m_voteId; }
const std::string& GovernanceVoteRecord::governanceProposalId() const {
    return m_governanceProposalId;
}
const std::string& GovernanceVoteRecord::voterId() const { return m_voterId; }
GovernanceVoteChoice GovernanceVoteRecord::choice() const { return m_choice; }
std::uint64_t GovernanceVoteRecord::votingPower() const { return m_votingPower; }
std::uint64_t GovernanceVoteRecord::votedAtBlock() const { return m_votedAtBlock; }
const std::string& GovernanceVoteRecord::policyVersion() const {
    return m_policyVersion;
}
bool GovernanceVoteRecord::isValid() const { return m_valid; }
const std::string& GovernanceVoteRecord::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceVoteRecord::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVoteRecord{"
        << "voteId=" << m_voteId
        << ";governanceProposalId=" << m_governanceProposalId
        << ";voterId=" << m_voterId
        << ";choice=" << governanceVoteChoiceToString(m_choice)
        << ";votingPower=" << m_votingPower
        << ";votedAtBlock=" << m_votedAtBlock
        << ";policyVersion=" << m_policyVersion
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
