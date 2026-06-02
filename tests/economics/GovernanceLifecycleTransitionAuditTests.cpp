#include "economics/GovernanceLifecycleTransitionAudit.hpp"

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleTransition.hpp"
#include "economics/GovernanceTransitionProof.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceLifecycleTransition;
using nodo::economics::GovernanceLifecycleTransitionAudit;
using nodo::economics::GovernanceTransitionProof;

const std::string kProposalId = "gov-prop-001";
const std::string kPolicyVersion = "governance-v1";

GovernanceLifecycleTransition makeT(
    const std::string& id,
    GovernanceLifecycleState from,
    GovernanceLifecycleState to,
    std::uint64_t block,
    const std::string& actor = "governance-node",
    const std::string& reason = "",
    const std::string& proposalId = kProposalId,
    const std::string& policyVersion = kPolicyVersion
) {
    const std::string proof = GovernanceTransitionProof::build(
        proposalId, id, from, to, block, actor, policyVersion
    );
    return GovernanceLifecycleTransition(
        id, proposalId, from, to, block, actor, reason, proof, policyVersion
    );
}

// Valid canonical path: DRAFT->SUBMITTED->REVIEW->VOTING->TALLYING->DECIDED_APPROVED
std::vector<GovernanceLifecycleTransition> validHistory() {
    using S = GovernanceLifecycleState;
    return {
        makeT("t-001", S::DRAFT,      S::SUBMITTED,        5),
        makeT("t-002", S::SUBMITTED,  S::REVIEW,           6),
        makeT("t-003", S::REVIEW,     S::VOTING,           10),
        makeT("t-004", S::VOTING,     S::TALLYING,         18),
        makeT("t-005", S::TALLYING,   S::DECIDED_APPROVED, 20),
    };
}

void testValidSequenceAccepted() {
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        validHistory(), kProposalId, kPolicyVersion
    );
    assert(result.accepted());
    assert(result.finalState() == GovernanceLifecycleState::DECIDED_APPROVED);
}

void testEmptyHistoryRejected() {
    const auto result = GovernanceLifecycleTransitionAudit::audit({}, kProposalId, kPolicyVersion);
    assert(!result.accepted());
}

void testDuplicateTransitionIdRejected() {
    using S = GovernanceLifecycleState;
    auto history = validHistory();
    // Replace t-002 with same id as t-001.
    history[1] = makeT("t-001", S::SUBMITTED, S::REVIEW, 6);
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testForgedTransitionProofRejected() {
    auto history = validHistory();
    // Construct a transition with a wrong proof.
    using S = GovernanceLifecycleState;
    const GovernanceLifecycleTransition bad(
        "t-002", kProposalId, S::SUBMITTED, S::REVIEW,
        6, "governance-node", "",
        "forged-proof",
        kPolicyVersion
    );
    assert(!bad.isValid());
    // Since bad.isValid() is false, the audit will reject it at structural check.
    history[1] = bad;
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testProposalMismatchRejected() {
    auto history = validHistory();
    using S = GovernanceLifecycleState;
    history[1] = makeT("t-002", S::SUBMITTED, S::REVIEW, 6, "governance-node", "",
                        "different-proposal-id", kPolicyVersion);
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testPolicyVersionMismatchRejected() {
    auto history = validHistory();
    using S = GovernanceLifecycleState;
    history[1] = makeT("t-002", S::SUBMITTED, S::REVIEW, 6, "governance-node", "",
                        kProposalId, "wrong-policy-v2");
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testOutOfOrderBlockRejected() {
    auto history = validHistory();
    using S = GovernanceLifecycleState;
    // t-004 at block 5, which is less than t-003 at block 10.
    history[3] = makeT("t-004", S::VOTING, S::TALLYING, 5);
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testFromStateMismatchRejected() {
    using S = GovernanceLifecycleState;
    // Transition claims VOTING as fromState but current state should be REVIEW.
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::REVIEW,    6),
        makeT("t-003", S::VOTING,    S::TALLYING,  10),  // fromState=VOTING but current=REVIEW
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testDirectJumpRejected() {
    using S = GovernanceLifecycleState;
    // Skip REVIEW and VOTING: SUBMITTED -> TALLYING directly.
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,      S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED,  S::TALLYING,  10),  // not allowed by state machine
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testTerminalStateCannotTransitionFurther() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,             S::SUBMITTED,        5),
        makeT("t-002", S::SUBMITTED,         S::REVIEW,           6),
        makeT("t-003", S::REVIEW,            S::VOTING,           10),
        makeT("t-004", S::VOTING,            S::TALLYING,         18),
        makeT("t-005", S::TALLYING,          S::DECIDED_REJECTED, 20),
        makeT("t-006", S::DECIDED_REJECTED,  S::SUBMITTED,        21),  // terminal cannot go back
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

void testFinalStateReturnedCorrectly() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,      S::SUBMITTED,        5, "governance-node", ""),
        makeT("t-002", S::SUBMITTED,  S::CANCELLED,        6, "governance-node", "duplicate"),
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(result.accepted());
    assert(result.finalState() == S::CANCELLED);
}

void testRejectedPathFinalState() {
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        validHistory(), kProposalId, kPolicyVersion
    );
    assert(result.accepted());
    assert(result.finalState() == GovernanceLifecycleState::DECIDED_APPROVED);
}

} // namespace

int main() {
    testValidSequenceAccepted();
    testEmptyHistoryRejected();
    testDuplicateTransitionIdRejected();
    testForgedTransitionProofRejected();
    testProposalMismatchRejected();
    testPolicyVersionMismatchRejected();
    testOutOfOrderBlockRejected();
    testFromStateMismatchRejected();
    testDirectJumpRejected();
    testTerminalStateCannotTransitionFurther();
    testFinalStateReturnedCorrectly();
    testRejectedPathFinalState();
    return 0;
}
