#include "node/GovernanceTallyService.hpp"

#include <algorithm>
#include <sstream>

namespace nodo::node {

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice) {
    switch (choice) {
        case GovernanceVoteChoice::YES:     return "YES";
        case GovernanceVoteChoice::NO:      return "NO";
        case GovernanceVoteChoice::ABSTAIN: return "ABSTAIN";
        default:                            return "ABSTAIN";
    }
}

bool governanceVoteChoiceFromString(
    const std::string& s,
    GovernanceVoteChoice& out
) {
    if (s == "YES")     { out = GovernanceVoteChoice::YES;     return true; }
    if (s == "NO")      { out = GovernanceVoteChoice::NO;      return true; }
    if (s == "ABSTAIN") { out = GovernanceVoteChoice::ABSTAIN; return true; }
    return false;
}

GovernanceTallyResult::GovernanceTallyResult()
    : m_proposalId("")
    , m_totalVotes(0)
    , m_yesVotes(0)
    , m_noVotes(0)
    , m_abstainVotes(0)
    , m_eligibleVoters(0)
    , m_outcome(economics::GovernanceLifecycleState::EXPIRED)
{}

GovernanceTallyResult::GovernanceTallyResult(
    std::string              proposalId,
    std::size_t              totalVotes,
    std::size_t              yesVotes,
    std::size_t              noVotes,
    std::size_t              abstainVotes,
    std::size_t              eligibleVoters,
    economics::GovernanceLifecycleState outcome
)
    : m_proposalId(std::move(proposalId))
    , m_totalVotes(totalVotes)
    , m_yesVotes(yesVotes)
    , m_noVotes(noVotes)
    , m_abstainVotes(abstainVotes)
    , m_eligibleVoters(eligibleVoters)
    , m_outcome(outcome)
{}

const std::string& GovernanceTallyResult::proposalId()     const { return m_proposalId; }
std::size_t        GovernanceTallyResult::totalVotes()     const { return m_totalVotes; }
std::size_t        GovernanceTallyResult::yesVotes()       const { return m_yesVotes; }
std::size_t        GovernanceTallyResult::noVotes()        const { return m_noVotes; }
std::size_t        GovernanceTallyResult::abstainVotes()   const { return m_abstainVotes; }
std::size_t        GovernanceTallyResult::eligibleVoters() const { return m_eligibleVoters; }
economics::GovernanceLifecycleState GovernanceTallyResult::outcome() const { return m_outcome; }
bool GovernanceTallyResult::approved() const {
    return m_outcome == economics::GovernanceLifecycleState::DECIDED_APPROVED;
}

std::string GovernanceTallyResult::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceTallyResult{"
        << "proposalId=" << m_proposalId
        << ";totalVotes=" << m_totalVotes
        << ";yes=" << m_yesVotes
        << ";no=" << m_noVotes
        << ";abstain=" << m_abstainVotes
        << ";eligible=" << m_eligibleVoters
        << ";outcome=" << economics::governanceLifecycleStateToString(m_outcome)
        << "}";
    return oss.str();
}

GovernanceTallyResult GovernanceTallyService::tally(
    const std::string&              proposalId,
    const std::vector<OnChainVote>& votes,
    const std::vector<std::string>& eligibleVoterAddresses
) {
    std::size_t yes     = 0;
    std::size_t no      = 0;
    std::size_t abstain = 0;

    for (const auto& vote : votes) {
        if (vote.proposalId != proposalId) continue;

        // Only count votes from eligible voters.
        const bool isEligible = std::find(
            eligibleVoterAddresses.begin(),
            eligibleVoterAddresses.end(),
            vote.voterAddress
        ) != eligibleVoterAddresses.end();

        if (!isEligible) continue;

        switch (vote.choice) {
            case GovernanceVoteChoice::YES:     ++yes;     break;
            case GovernanceVoteChoice::NO:      ++no;      break;
            case GovernanceVoteChoice::ABSTAIN: ++abstain; break;
        }
    }

    const std::size_t eligible = eligibleVoterAddresses.size();
    const std::size_t total    = yes + no + abstain;

    economics::GovernanceLifecycleState outcome;

    if (eligible == 0 || total == 0) {
        outcome = economics::GovernanceLifecycleState::EXPIRED;
    } else {
        const double quorum   = static_cast<double>(total)  / static_cast<double>(eligible);
        const double approval = static_cast<double>(yes)    / static_cast<double>(total);

        if (quorum < QUORUM_THRESHOLD) {
            outcome = economics::GovernanceLifecycleState::EXPIRED;
        } else if (approval > APPROVAL_THRESHOLD) {
            outcome = economics::GovernanceLifecycleState::DECIDED_APPROVED;
        } else {
            outcome = economics::GovernanceLifecycleState::DECIDED_REJECTED;
        }
    }

    return GovernanceTallyResult(proposalId, total, yes, no, abstain, eligible, outcome);
}

bool GovernanceTallyService::hasDuplicateVote(
    const std::string&              voterAddress,
    const std::string&              proposalId,
    const std::vector<OnChainVote>& existingVotes
) {
    for (const auto& v : existingVotes) {
        if (v.proposalId == proposalId && v.voterAddress == voterAddress) {
            return true;
        }
    }
    return false;
}

} // namespace nodo::node
