#include "economics/GovernanceTallyReport.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

using nodo::economics::GovernanceTallyReport;
using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteSetAudit;
using nodo::economics::GovernanceVotingPolicy;
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

GovernanceVotingPolicy policy(
    std::int64_t quorum,
    std::uint32_t threshold,
    bool allowAbstain = true
) {
    return GovernanceVotingPolicy(
        "governance-v1",
        Amount::fromRawUnits(quorum),
        threshold,
        Amount::fromRawUnits(1),
        allowAbstain,
        false
    );
}

GovernanceTallyReport tallyFor(
    const std::vector<GovernanceVoteEvidence>& votes,
    const GovernanceVotingPolicy& votingPolicy
) {
    const auto audit =
        GovernanceVoteSetAudit::audit("gov-prop-001", votingPolicy, votes);
    return GovernanceTallyReport::build(audit, votingPolicy);
}

void testValidTallySumsVotes() {
    const auto votingPolicy = policy(100, 6000);
    const std::vector<GovernanceVoteEvidence> votes = {
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 60),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 20),
        voteEvidence("vote-003", "validator-c", GovernanceVoteChoice::ABSTAIN, 20)
    };
    const auto tally = tallyFor(votes, votingPolicy);
    assert(tally.isValid());
    assert(tally.totalVotingPower() == Amount::fromRawUnits(100));
    assert(tally.yesVotingPower() == Amount::fromRawUnits(60));
    assert(tally.noVotingPower() == Amount::fromRawUnits(20));
    assert(tally.abstainVotingPower() == Amount::fromRawUnits(20));
    assert(tally.voteCount() == 3);
}

void testQuorumNotMetRejectsApproval() {
    const auto votingPolicy = policy(100, 6000);
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 50)
    }, votingPolicy);
    assert(tally.isValid());
    assert(!tally.quorumMet());
    assert(!tally.approved());
}

void testThresholdNotMetRejectsApproval() {
    const auto votingPolicy = policy(100, 6000);
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 40),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 60)
    }, votingPolicy);
    assert(tally.isValid());
    assert(tally.quorumMet());
    assert(!tally.approvalThresholdMet());
    assert(!tally.approved());
}

void testThresholdMetApproves() {
    const auto votingPolicy = policy(100, 6000);
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 60),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 20),
        voteEvidence("vote-003", "validator-c", GovernanceVoteChoice::ABSTAIN, 20)
    }, votingPolicy);
    assert(tally.isValid());
    assert(tally.quorumMet());
    assert(tally.approvalThresholdMet());
    assert(tally.approved());
}

void testAbstainExcludedFromApprovalRatio() {
    const auto votingPolicy = policy(100, 6000);
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 60),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 20),
        voteEvidence("vote-003", "validator-c", GovernanceVoteChoice::ABSTAIN, 920)
    }, votingPolicy);
    assert(tally.isValid());
    assert(tally.approvalThresholdMet());
    assert(tally.approved());
}

void testZeroYesNoDoesNotApprove() {
    const auto votingPolicy = policy(100, 6000);
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::ABSTAIN, 100)
    }, votingPolicy);
    assert(tally.isValid());
    assert(!tally.approvalThresholdMet());
    assert(!tally.approved());
}

void testTamperedTallyProofRejectedByConstructor() {
    const GovernanceTallyReport tally(
        "gov-prop-001",
        "governance-v1",
        100,
        60,
        20,
        20,
        1,
        1,
        1,
        true,
        true,
        true,
        "forged-proof"
    );
    assert(!tally.isValid());
}

} // namespace

int main() {
    testValidTallySumsVotes();
    testQuorumNotMetRejectsApproval();
    testThresholdNotMetRejectsApproval();
    testThresholdMetApproves();
    testAbstainExcludedFromApprovalRatio();
    testZeroYesNoDoesNotApprove();
    testTamperedTallyProofRejectedByConstructor();
    return 0;
}
