#include "economics/GovernanceLifecycleTransition.hpp"

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceTransitionProof.hpp"

#include <cassert>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceLifecycleTransition;
using nodo::economics::GovernanceTransitionProof;

GovernanceLifecycleTransition makeValid(
    const std::string& transitionId = "trans-001",
    GovernanceLifecycleState from = GovernanceLifecycleState::DRAFT,
    GovernanceLifecycleState to = GovernanceLifecycleState::SUBMITTED,
    std::uint64_t block = 5,
    const std::string& actor = "actor-node",
    const std::string& reason = "",
    const std::string& proposalId = "gov-prop-001",
    const std::string& policyVersion = "governance-v1"
) {
    const std::string proof = GovernanceTransitionProof::build(
        proposalId, transitionId, from, to, block, actor, policyVersion
    );
    return GovernanceLifecycleTransition(
        transitionId, proposalId, from, to, block, actor, reason, proof, policyVersion
    );
}

void testValidTransitionAccepted() {
    const auto t = makeValid();
    assert(t.isValid());
    assert(t.transitionId() == "trans-001");
    assert(t.fromState() == GovernanceLifecycleState::DRAFT);
    assert(t.toState() == GovernanceLifecycleState::SUBMITTED);
}

void testEmptyTransitionIdRejected() {
    const std::string proof = GovernanceTransitionProof::build(
        "gov-prop-001", "", GovernanceLifecycleState::DRAFT,
        GovernanceLifecycleState::SUBMITTED, 5, "actor", "v1"
    );
    const GovernanceLifecycleTransition t(
        "", "gov-prop-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "", proof, "v1"
    );
    assert(!t.isValid());
}

void testEmptyProposalIdRejected() {
    const GovernanceLifecycleTransition t(
        "trans-001", "",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "", "some-proof", "v1"
    );
    assert(!t.isValid());
}

void testEmptyActorRejected() {
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "", "", "some-proof", "v1"
    );
    assert(!t.isValid());
}

void testEmptyProofRejected() {
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "", "", "v1"
    );
    assert(!t.isValid());
}

void testEmptyPolicyVersionRejected() {
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "", "some-proof", ""
    );
    assert(!t.isValid());
}

void testCancelledWithoutReasonRejected() {
    const std::string proof = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::SUBMITTED, GovernanceLifecycleState::CANCELLED,
        10, "actor", "v1"
    );
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::SUBMITTED, GovernanceLifecycleState::CANCELLED,
        10, "actor", "", proof, "v1"
    );
    assert(!t.isValid());
}

void testExpiredWithoutReasonRejected() {
    const std::string proof = GovernanceTransitionProof::build(
        "gov-prop-001", "trans-001",
        GovernanceLifecycleState::REVIEW, GovernanceLifecycleState::EXPIRED,
        15, "actor", "v1"
    );
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::REVIEW, GovernanceLifecycleState::EXPIRED,
        15, "actor", "", proof, "v1"
    );
    assert(!t.isValid());
}

void testCancelledWithReasonAccepted() {
    const auto t = makeValid(
        "trans-001",
        GovernanceLifecycleState::SUBMITTED, GovernanceLifecycleState::CANCELLED,
        10, "actor", "duplicate proposal"
    );
    assert(t.isValid());
}

void testForgedProofRejected() {
    const GovernanceLifecycleTransition t(
        "trans-001", "gov-prop-001",
        GovernanceLifecycleState::DRAFT, GovernanceLifecycleState::SUBMITTED,
        5, "actor", "", "forged-proof", "v1"
    );
    assert(!t.isValid());
}

void testDefaultConstructedIsInvalid() {
    const GovernanceLifecycleTransition t;
    assert(!t.isValid());
}

} // namespace

int main() {
    testValidTransitionAccepted();
    testEmptyTransitionIdRejected();
    testEmptyProposalIdRejected();
    testEmptyActorRejected();
    testEmptyProofRejected();
    testEmptyPolicyVersionRejected();
    testCancelledWithoutReasonRejected();
    testExpiredWithoutReasonRejected();
    testCancelledWithReasonAccepted();
    testForgedProofRejected();
    testDefaultConstructedIsInvalid();
    return 0;
}
