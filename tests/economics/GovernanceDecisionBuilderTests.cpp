#include "economics/GovernanceDecisionBuilder.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"

#include <cassert>
#include <vector>

namespace {

using nodo::economics::GovernanceDecisionBuilder;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernanceTallyReport;
using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteSetAudit;
using nodo::economics::GovernanceVotingPolicy;
using nodo::tests::fixtures::validEnvelope;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

GovernanceTallyReport tallyFor(std::vector<GovernanceVoteEvidence> votes) {
    const auto audit =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(audit.accepted());
    const auto tally = GovernanceTallyReport::build(audit, validVotingPolicy());
    assert(tally.isValid());
    return tally;
}

void testApprovedTallyProducesApprovedDecision() {
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 60),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 20),
        voteEvidence("vote-003", "validator-c", GovernanceVoteChoice::ABSTAIN, 20)
    });
    const auto result = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    assert(result.built());
    assert(result.decisionRecord().decisionStatus() ==
           GovernanceDecisionStatus::APPROVED);
}

void testRejectedTallyProducesRejectedDecision() {
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::NO, 100)
    });
    const auto result = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    assert(result.built());
    assert(result.decisionRecord().decisionStatus() ==
           GovernanceDecisionStatus::REJECTED);
}

void testProposalMismatchRejected() {
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 100)
    });
    const auto result = GovernanceDecisionBuilder::buildDecision(
        validEnvelope("prop-002", "gov-prop-002"),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    assert(!result.built());
}

void testPolicyMismatchRejected() {
    const GovernanceVotingPolicy policyV2(
        "governance-v2",
        Amount::fromRawUnits(100),
        6000,
        Amount::fromRawUnits(1),
        true,
        false
    );
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 100)
    });
    const auto result = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        policyV2,
        tally,
        20,
        "governance-node"
    );
    assert(!result.built());
}

void testDecidedBeforeSubmissionRejected() {
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 100)
    });
    const auto result = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        1,
        "governance-node"
    );
    assert(!result.built());
}

void testDecisionProofDeterministic() {
    const auto tally = tallyFor({
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 100)
    });
    const auto first = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    const auto second = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    assert(first.built());
    assert(second.built());
    assert(first.decisionRecord().decisionProof() ==
           second.decisionRecord().decisionProof());
}

} // namespace

int main() {
    testApprovedTallyProducesApprovedDecision();
    testRejectedTallyProducesRejectedDecision();
    testProposalMismatchRejected();
    testPolicyMismatchRejected();
    testDecidedBeforeSubmissionRejected();
    testDecisionProofDeterministic();
    return 0;
}
