#include "economics/GovernanceVoteSetAudit.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceVoteProof.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::economics::GovernanceVoteSetAudit;
using nodo::economics::GovernanceVoteSetAuditStatus;
using nodo::economics::GovernanceVotingPolicy;
using nodo::tests::fixtures::validEnvelope;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

GovernanceVoteEvidence makeEvidence(
    const std::string& evidenceId,
    const std::string& voteId,
    const std::string& voterId,
    const GovernanceVotingPolicy& policy = validVotingPolicy(),
    const std::string& governanceProposalId = "gov-prop-001",
    GovernanceVoteChoice choice = GovernanceVoteChoice::YES,
    std::uint64_t castAtBlock = 12
) {
    const std::string proof = GovernanceVoteProof::build(
        governanceProposalId,
        voterId,
        choice,
        Amount::fromRawUnits(50),
        castAtBlock,
        policy.policyVersion()
    );
    GovernanceVoteRecord record(
        voteId,
        governanceProposalId,
        voterId,
        choice,
        Amount::fromRawUnits(50),
        castAtBlock,
        "validator-stake",
        proof,
        policy.policyVersion()
    );
    return GovernanceVoteEvidence(
        evidenceId,
        validEnvelope("prop-001", governanceProposalId),
        policy,
        std::move(record)
    );
}

void testValidVoteSetAccepted() {
    const std::vector<GovernanceVoteEvidence> votes = {
        voteEvidence("vote-002", "validator-b", GovernanceVoteChoice::YES, 50),
        voteEvidence("vote-001", "validator-a", GovernanceVoteChoice::YES, 50)
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(result.accepted());
    assert(result.canonicalVotes()[0].voteRecord().voteId() == "vote-001");
    assert(result.canonicalVotes()[1].voteRecord().voteId() == "vote-002");
}

void testDuplicateEvidenceIdRejected() {
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence("evidence-dup", "vote-001", "validator-a"),
        makeEvidence("evidence-dup", "vote-002", "validator-b")
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(!result.accepted());
    assert(result.status() == GovernanceVoteSetAuditStatus::DUPLICATE_EVIDENCE_ID);
}

void testDuplicateVoteIdRejected() {
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence("evidence-001", "vote-dup", "validator-a"),
        makeEvidence("evidence-002", "vote-dup", "validator-b")
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(!result.accepted());
    assert(result.status() == GovernanceVoteSetAuditStatus::DUPLICATE_VOTE_ID);
}

void testDuplicateVoterRejected() {
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence("evidence-001", "vote-001", "validator-a"),
        makeEvidence("evidence-002", "vote-002", "validator-a")
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(!result.accepted());
    assert(result.status() == GovernanceVoteSetAuditStatus::DUPLICATE_VOTER);
}

void testVoteForDifferentProposalRejected() {
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence("evidence-001", "vote-001", "validator-a", validVotingPolicy(), "gov-other")
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(!result.accepted());
    assert(result.status() == GovernanceVoteSetAuditStatus::PROPOSAL_MISMATCH);
}

void testPolicyMismatchRejected() {
    const GovernanceVotingPolicy otherPolicy(
        "governance-v1",
        Amount::fromRawUnits(101),
        6000,
        Amount::fromRawUnits(1),
        true,
        false
    );
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence("evidence-001", "vote-001", "validator-a", otherPolicy)
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", validVotingPolicy(), votes);
    assert(!result.accepted());
    assert(result.status() == GovernanceVoteSetAuditStatus::POLICY_VERSION_MISMATCH);
}

void testReplacementFlagBehaviorExplicit() {
    const GovernanceVotingPolicy replacementPolicy(
        "governance-v1",
        Amount::fromRawUnits(100),
        6000,
        Amount::fromRawUnits(1),
        true,
        true
    );
    const std::vector<GovernanceVoteEvidence> votes = {
        makeEvidence(
            "evidence-001",
            "vote-001",
            "validator-a",
            replacementPolicy,
            "gov-prop-001",
            GovernanceVoteChoice::NO,
            12
        ),
        makeEvidence(
            "evidence-002",
            "vote-002",
            "validator-a",
            replacementPolicy,
            "gov-prop-001",
            GovernanceVoteChoice::YES,
            13
        )
    };
    const auto result =
        GovernanceVoteSetAudit::audit("gov-prop-001", replacementPolicy, votes);
    assert(result.accepted());
    assert(result.canonicalVotes().size() == 1);
    assert(result.canonicalVotes()[0].voteRecord().voteId() == "vote-002");
    assert(result.canonicalVotes()[0].voteRecord().voteChoice() == GovernanceVoteChoice::YES);
}

} // namespace

int main() {
    testValidVoteSetAccepted();
    testDuplicateEvidenceIdRejected();
    testDuplicateVoteIdRejected();
    testDuplicateVoterRejected();
    testVoteForDifferentProposalRejected();
    testPolicyMismatchRejected();
    testReplacementFlagBehaviorExplicit();
    return 0;
}
