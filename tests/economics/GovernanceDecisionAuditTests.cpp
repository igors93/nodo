#include "economics/GovernanceDecisionAudit.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceTallyReport.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"

#include <cassert>
#include <utility>
#include <vector>

namespace {

using nodo::economics::GovernanceDecisionAudit;
using nodo::economics::GovernanceDecisionAuditStatus;
using nodo::economics::GovernanceDecisionBuilder;
using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernanceTallyReport;
using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::economics::GovernanceVoteSetAudit;
using nodo::tests::fixtures::validEnvelope;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

GovernanceDecisionRecord buildDecision(
    const std::vector<GovernanceVoteEvidence>& votes
) {
    const auto audit =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(audit.accepted());
    const auto tally = GovernanceTallyReport::build(audit, validVotingPolicy());
    assert(tally.isValid());
    const auto build = GovernanceDecisionBuilder::buildDecision(
        validEnvelope(),
        validVotingPolicy(),
        tally,
        20,
        "governance-node"
    );
    assert(build.built());
    return build.decisionRecord();
}

std::vector<GovernanceVoteEvidence> validVotes() {
    return {
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 60),
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::NO, 20),
        voteEvidence("vote-003", "validator-c", GovernanceVoteChoice::ABSTAIN, 20)
    };
}

void testValidGovernanceDecisionAuditPasses() {
    const auto votes = validVotes();
    const auto decision = buildDecision(votes);
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        decision
    );
    assert(result.accepted());
}

void testDuplicateVoteRejected() {
    auto votes = validVotes();
    votes[1] = voteEvidence(
        "vote-004",
        "validator-a",
        GovernanceVoteChoice::YES,
        20
    );
    const auto decision = validLifecycle().decisionRecord();
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        decision
    );
    assert(!result.accepted());
    assert(result.status() == GovernanceDecisionAuditStatus::VOTE_AUDIT_FAILED);
}

void testForgedVoteProofRejected() {
    auto votes = validVotes();
    GovernanceVoteRecord forged(
        "vote-001",
        "gov-prop-001",
        "validator-a",
        GovernanceVoteChoice::YES,
        Amount::fromRawUnits(60),
        12,
        "validator-stake",
        "forged-proof",
        "governance-v1"
    );
    votes[0] = GovernanceVoteEvidence(
        "evidence-vote-001",
        validEnvelope(),
        validVotingPolicy(),
        std::move(forged)
    );
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        validLifecycle().decisionRecord()
    );
    assert(!result.accepted());
    assert(result.status() == GovernanceDecisionAuditStatus::VOTE_AUDIT_FAILED);
}

void testForgedDecisionProofRejected() {
    const auto votes = validVotes();
    const auto decision = buildDecision(votes);
    const GovernanceDecisionRecord forged(
        decision.decisionId(),
        decision.governanceProposalId(),
        decision.proposalType(),
        decision.decisionStatus(),
        decision.decidedAtBlock(),
        decision.decisionMaker(),
        "forged-decision-proof",
        decision.policyVersion()
    );
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        forged
    );
    assert(!result.accepted());
    assert(result.status() ==
           GovernanceDecisionAuditStatus::DECISION_PROOF_MISMATCH);
}

void testDecisionStatusMismatchRejected() {
    const auto votes = validVotes();
    const auto decision = buildDecision(votes);
    const GovernanceDecisionRecord forged(
        decision.decisionId(),
        decision.governanceProposalId(),
        decision.proposalType(),
        GovernanceDecisionStatus::REJECTED,
        decision.decidedAtBlock(),
        decision.decisionMaker(),
        decision.decisionProof(),
        decision.policyVersion()
    );
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        forged
    );
    assert(!result.accepted());
    assert(result.status() ==
           GovernanceDecisionAuditStatus::DECISION_STATUS_MISMATCH);
}

void testDecisionPolicyVersionMismatchRejected() {
    const auto votes = validVotes();
    const auto decision = buildDecision(votes);
    const GovernanceDecisionRecord forged(
        decision.decisionId(),
        decision.governanceProposalId(),
        decision.proposalType(),
        decision.decisionStatus(),
        decision.decidedAtBlock(),
        decision.decisionMaker(),
        decision.decisionProof(),
        "governance-v2"
    );
    const auto result = GovernanceDecisionAudit::auditDecision(
        validEnvelope(),
        validVotingPolicy(),
        votes,
        forged
    );
    assert(!result.accepted());
    assert(result.status() ==
           GovernanceDecisionAuditStatus::DECISION_POLICY_VERSION_MISMATCH);
}

} // namespace

int main() {
    testValidGovernanceDecisionAuditPasses();
    testDuplicateVoteRejected();
    testForgedVoteProofRejected();
    testForgedDecisionProofRejected();
    testDecisionStatusMismatchRejected();
    testDecisionPolicyVersionMismatchRejected();
    return 0;
}
