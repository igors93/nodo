#ifndef NODO_NODE_GOVERNANCE_TALLY_SERVICE_HPP
#define NODO_NODE_GOVERNANCE_TALLY_SERVICE_HPP

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * GovernanceVoteRecord represents one on-chain vote cast via a
 * GOVERNANCE_VOTE transaction.
 */
enum class GovernanceVoteChoice {
    YES,
    NO,
    ABSTAIN
};

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice);
bool governanceVoteChoiceFromString(const std::string& s, GovernanceVoteChoice& out);

struct OnChainVote {
    std::string         proposalId;
    std::string         voterAddress;
    GovernanceVoteChoice choice;
    std::uint64_t       blockHeight;
    std::string         txId;
};

/*
 * GovernanceTallyResult holds the outcome of a vote tally over a proposal.
 */
class GovernanceTallyResult {
public:
    GovernanceTallyResult();

    GovernanceTallyResult(
        std::string              proposalId,
        std::size_t              totalVotes,
        std::size_t              yesVotes,
        std::size_t              noVotes,
        std::size_t              abstainVotes,
        std::size_t              eligibleVoters,
        economics::GovernanceLifecycleState outcome
    );

    const std::string& proposalId()       const;
    std::size_t        totalVotes()       const;
    std::size_t        yesVotes()         const;
    std::size_t        noVotes()          const;
    std::size_t        abstainVotes()     const;
    std::size_t        eligibleVoters()   const;

    economics::GovernanceLifecycleState outcome() const;
    bool                                approved() const;

    std::string serialize() const;

private:
    std::string              m_proposalId;
    std::size_t              m_totalVotes;
    std::size_t              m_yesVotes;
    std::size_t              m_noVotes;
    std::size_t              m_abstainVotes;
    std::size_t              m_eligibleVoters;
    economics::GovernanceLifecycleState m_outcome;
};

/*
 * GovernanceTallyService tallies votes collected for a proposal and
 * determines the lifecycle outcome.
 *
 * Approval rules (conservative testnet defaults):
 * - Quorum:   at least 50% of eligible voters must cast a vote.
 * - Majority: more than 50% of participating votes must be YES.
 * - Expired:  if voting period has elapsed with insufficient quorum,
 *             the proposal expires.
 *
 * Security principle:
 * The tally is deterministic given the input votes and eligible voter set.
 * The same inputs always produce the same outcome, making the result
 * auditable and re-computable from chain history.
 */
class GovernanceTallyService {
public:
    static constexpr double QUORUM_THRESHOLD   = 0.50;
    static constexpr double APPROVAL_THRESHOLD = 0.50;

    static GovernanceTallyResult tally(
        const std::string&           proposalId,
        const std::vector<OnChainVote>& votes,
        const std::vector<std::string>& eligibleVoterAddresses
    );

    /*
     * Returns true if a vote from voterAddress for proposalId already exists.
     */
    static bool hasDuplicateVote(
        const std::string&              voterAddress,
        const std::string&              proposalId,
        const std::vector<OnChainVote>& existingVotes
    );
};

} // namespace nodo::node

#endif
