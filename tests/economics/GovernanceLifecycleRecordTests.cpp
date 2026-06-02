#include "economics/GovernanceLifecycleRecord.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include <cassert>
#include <vector>

namespace {

using nodo::economics::GovernanceLifecycleRecord;
using nodo::tests::fixtures::validGovernancePolicy;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::validVotingPolicy;

void testValidLifecycleAccepted() {
    const GovernanceLifecycleRecord lifecycle = validLifecycle();
    assert(lifecycle.isValid());
    assert(lifecycle.lifecycleId() == "lifecycle-001");
    assert(lifecycle.voteEvidenceList().size() == 3);
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
        base.finalizedAtBlock()
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
        base.finalizedAtBlock()
    );
    assert(!invalid.isValid());
}

void testPolicyVersionMismatchRejected() {
    const GovernanceLifecycleRecord base = validLifecycle();
    const GovernanceLifecycleRecord invalid(
        "lifecycle-policy-mismatch",
        base.proposalEnvelope(),
        validGovernancePolicy(),
        nodo::economics::GovernanceVotingPolicy("governance-v2", 100, 60, true, true),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock()
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
        5
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
    return 0;
}
