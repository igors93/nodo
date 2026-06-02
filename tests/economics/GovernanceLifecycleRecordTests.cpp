#include "economics/GovernanceLifecycleRecord.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include <cassert>
#include <vector>

namespace {

using nodo::economics::GovernanceLifecycleRecord;
using nodo::economics::GovernanceLifecycleState;
using nodo::tests::fixtures::validGovernancePolicy;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::utils::Amount;

void testValidLifecycleAccepted() {
    const GovernanceLifecycleRecord lifecycle = validLifecycle();
    assert(lifecycle.isValid());
    assert(lifecycle.lifecycleId() == "lifecycle-001");
    assert(lifecycle.voteEvidenceList().size() == 3);
    assert(lifecycle.declaredCurrentState() == GovernanceLifecycleState::DECIDED_APPROVED);
    assert(!lifecycle.transitionHistory().empty());
}

void testEmptyLifecycleIdRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        base.declaredCurrentState(),
        base.transitionHistory()
    );
    assert(!invalid.isValid());
}

void testMissingVotesRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "lifecycle-empty-votes",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        {},
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        base.declaredCurrentState(),
        base.transitionHistory()
    );
    assert(!invalid.isValid());
}

void testPolicyVersionMismatchRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "lifecycle-policy-mismatch",
        base.proposalEnvelope(),
        validGovernancePolicy(),
        nodo::economics::GovernanceVotingPolicy(
            "governance-v2",
            Amount::fromRawUnits(100),
            6000,
            Amount::fromRawUnits(1),
            true,
            false
        ),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        base.declaredCurrentState(),
        base.transitionHistory()
    );
    assert(!invalid.isValid());
}

void testFinalizedBeforeCreatedRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "lifecycle-bad-height",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        20,
        5,
        base.declaredCurrentState(),
        base.transitionHistory()
    );
    assert(!invalid.isValid());
}

void testMissingTransitionHistoryRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "lifecycle-no-transitions",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        base.declaredCurrentState(),
        {}
    );
    assert(!invalid.isValid());
}

void testDeclaredStateMismatchRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    // Claim SUBMITTED but transitions actually reach DECIDED_APPROVED.
    const GovernanceLifecycleRecord invalid(
        "lifecycle-state-mismatch",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        GovernanceLifecycleState::SUBMITTED,
        base.transitionHistory()
    );
    assert(!invalid.isValid());
}

} // namespace

int main() {
    testValidLifecycleAccepted();
    testEmptyLifecycleIdRejected();
    testMissingVotesRejected();
    testPolicyVersionMismatchRejected();
    testFinalizedBeforeCreatedRejected();
    testMissingTransitionHistoryRejected();
    testDeclaredStateMismatchRejected();
    return 0;
}
