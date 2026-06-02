#include "economics/GovernanceTransitionProof.hpp"

#include "economics/GovernanceLifecycleState.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceTransitionProof;

void testSameInputsProduceSameProof() {
    const auto p1 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "submitter-node", "governance-v1"
    );
    const auto p2 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "submitter-node", "governance-v1"
    );
    assert(p1 == p2);
    assert(!p1.empty());
}

void testChangingProposalChangesProof() {
    const auto p1 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    const auto p2 = GovernanceTransitionProof::build(
        "gov-prop-002", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    assert(p1 != p2);
}

void testChangingStateChangesProof() {
    const auto p1 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    const auto p2 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::SUBMITTED, GovernanceLifecycleState::REVIEW,
        5, "actor", "v1"
    );
    assert(p1 != p2);
}

void testChangingBlockChangesProof() {
    const auto p1 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    const auto p2 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        6, "actor", "v1"
    );
    assert(p1 != p2);
}

void testChangingActorChangesProof() {
    const auto p1 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor-a", "v1"
    );
    const auto p2 = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor-b", "v1"
    );
    assert(p1 != p2);
}

void testVerifyRejectsForgedProof() {
    const auto correct = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    assert(GovernanceTransitionProof::verify(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1",
        correct
    ));
    assert(!GovernanceTransitionProof::verify(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1",
        "forged-proof"
    ));
    assert(!GovernanceTransitionProof::verify(
        "gov-prop-002", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1",
        correct
    ));
}

void testProofContainsExpectedPrefix() {
    const auto p = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "v1"
    );
    assert(p.find("governance-transition:") == 0);
}

} // namespace

int main() {
    testSameInputsProduceSameProof();
    testChangingProposalChangesProof();
    testChangingStateChangesProof();
    testChangingBlockChangesProof();
    testChangingActorChangesProof();
    testVerifyRejectsForgedProof();
    testProofContainsExpectedPrefix();
    return 0;
}
