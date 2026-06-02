#include "economics/GovernanceLifecycleVerifier.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVoteEvidence.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"

#include <cassert>
#include <vector>

namespace {

using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernanceLifecycleRecord;
using nodo::economics::GovernanceLifecycleVerificationStatus;
using nodo::economics::GovernanceLifecycleVerifier;
using nodo::economics::GovernanceTallyReport;
using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::voteEvidence;
using nodo::tests::fixtures::validVotes;

GovernanceLifecycleRecord withVotes(
    const GovernanceLifecycleRecord& base,
    std::vector<GovernanceVoteEvidence> votes
) {
    return GovernanceLifecycleRecord(
        "lifecycle-mutated",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        std::move(votes),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock()
    );
}

void testValidLifecycleVerifies() {
    const auto result = GovernanceLifecycleVerifier::verify(validLifecycle());
    assert(result.verified());
    assert(result.status() == GovernanceLifecycleVerificationStatus::VERIFIED);
}

void testDuplicateVoterFails() {
    const auto base = validLifecycle();
    auto votes = base.voteEvidenceList();
    votes[1] = voteEvidence(
        "vote-004",
        "validator-a",
        GovernanceVoteChoice::NO,
        20,
        "prop-001",
        "gov-prop-001",
        13
    );

    const auto result = GovernanceLifecycleVerifier::verify(withVotes(base, votes));
    assert(!result.verified());
    assert(result.status() == GovernanceLifecycleVerificationStatus::VOTE_AUDIT_FAILED);
}

void testChangeVoteChoiceAfterTallyFails() {
    const auto base = validLifecycle();
    auto votes = base.voteEvidenceList();
    votes[0] = voteEvidence(
        "vote-001",
        "validator-a",
        GovernanceVoteChoice::NO,
        60,
        "prop-001",
        "gov-prop-001",
        12
    );

    const auto result = GovernanceLifecycleVerifier::verify(withVotes(base, votes));
    assert(!result.verified());
    assert(result.status() == GovernanceLifecycleVerificationStatus::TALLY_MISMATCH ||
           result.status() == GovernanceLifecycleVerificationStatus::DECISION_AUDIT_FAILED);
}

void testChangeVotePowerAfterTallyFails() {
    const auto base = validLifecycle();
    auto votes = base.voteEvidenceList();
    votes[0] = voteEvidence(
        "vote-001",
        "validator-a",
        GovernanceVoteChoice::YES,
        61,
        "prop-001",
        "gov-prop-001",
        12
    );

    const auto result = GovernanceLifecycleVerifier::verify(withVotes(base, votes));
    assert(!result.verified());
}

void testTamperedTallyFails() {
    const auto base = validLifecycle();
    const GovernanceTallyReport tampered(
        "gov-prop-001",
        "governance-v1",
        100,
        61,
        19,
        20,
        1,
        1,
        1,
        true,
        true,
        true,
        GovernanceTallyReport::buildTallyProof(
            "gov-prop-001", "governance-v1", 100, 61, 19, 20,
            1, 1, 1, true, true, true
        )
    );
    const GovernanceLifecycleRecord lifecycle(
        "lifecycle-tampered-tally",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        tampered,
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock()
    );
    const auto result = GovernanceLifecycleVerifier::verify(lifecycle);
    assert(!result.verified());
    assert(result.status() == GovernanceLifecycleVerificationStatus::TALLY_MISMATCH);
}

void testTamperedDecisionProofFails() {
    const auto base = validLifecycle();
    const GovernanceDecisionRecord tampered(
        base.decisionRecord().decisionId(),
        base.decisionRecord().governanceProposalId(),
        base.decisionRecord().proposalType(),
        base.decisionRecord().decisionStatus(),
        base.decisionRecord().decidedAtBlock(),
        base.decisionRecord().decisionMaker(),
        "tampered-proof",
        base.decisionRecord().policyVersion()
    );
    const GovernanceLifecycleRecord lifecycle(
        "lifecycle-tampered-decision",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        tampered,
        base.createdAtBlock(),
        base.finalizedAtBlock()
    );
    const auto result = GovernanceLifecycleVerifier::verify(lifecycle);
    assert(!result.verified());
    assert(result.status() == GovernanceLifecycleVerificationStatus::DECISION_AUDIT_FAILED);
}

void testReuseTallyForAnotherProposalFails() {
    const auto base = validLifecycle();
    const auto other = validLifecycle("lifecycle-other", "prop-002", "gov-prop-002");
    const GovernanceLifecycleRecord lifecycle(
        "lifecycle-reused-tally",
        other.proposalEnvelope(),
        other.governancePolicy(),
        other.votingPolicy(),
        other.voteEvidenceList(),
        base.tallyReport(),
        other.decisionRecord(),
        other.createdAtBlock(),
        other.finalizedAtBlock()
    );
    assert(!lifecycle.isValid());
}

} // namespace

int main() {
    testValidLifecycleVerifies();
    testDuplicateVoterFails();
    testChangeVoteChoiceAfterTallyFails();
    testChangeVotePowerAfterTallyFails();
    testTamperedTallyFails();
    testTamperedDecisionProofFails();
    testReuseTallyForAnotherProposalFails();
    return 0;
}
