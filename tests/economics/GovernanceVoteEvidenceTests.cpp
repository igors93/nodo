#include "economics/GovernanceVoteEvidence.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceVoteProof.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::economics::GovernanceVotingPolicy;
using nodo::tests::fixtures::validEnvelope;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

GovernanceVoteRecord makeRecord(
    GovernanceVoteChoice choice,
    std::int64_t power,
    std::uint64_t castAtBlock,
    const std::string& policyVersion = "governance-v1",
    const std::string& proposalId = "gov-prop-001",
    const std::string& proofOverride = ""
) {
    const std::string proof = proofOverride.empty()
        ? GovernanceVoteProof::build(
              proposalId,
              "validator-a",
              choice,
              Amount::fromRawUnits(power),
              castAtBlock,
              policyVersion)
        : proofOverride;
    return GovernanceVoteRecord(
        "vote-001",
        proposalId,
        "validator-a",
        choice,
        Amount::fromRawUnits(power),
        castAtBlock,
        "validator-stake",
        proof,
        policyVersion
    );
}

void testValidVoteEvidenceAccepted() {
    assert(voteEvidence(
               "vote-001",
               "validator-a",
               GovernanceVoteChoice::YES,
               100
           ).isValid());
}

void testMissingEvidenceIdRejected() {
    const GovernanceVoteEvidence evidence(
        "",
        validEnvelope(),
        validVotingPolicy(),
        makeRecord(GovernanceVoteChoice::YES, 100, 12)
    );
    assert(!evidence.isValid());
}

void testVoteForDifferentProposalRejected() {
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope("prop-001", "gov-prop-999"),
        validVotingPolicy(),
        makeRecord(GovernanceVoteChoice::YES, 100, 12)
    );
    assert(!evidence.isValid());
}

void testPolicyVersionMismatchRejected() {
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope(),
        validVotingPolicy(),
        makeRecord(GovernanceVoteChoice::YES, 100, 12, "governance-v2")
    );
    assert(!evidence.isValid());
}

void testVoteBeforeProposalSubmissionRejected() {
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope(),
        validVotingPolicy(),
        makeRecord(GovernanceVoteChoice::YES, 100, 1)
    );
    assert(!evidence.isValid());
}

void testForgedVoteProofRejected() {
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope(),
        validVotingPolicy(),
        makeRecord(
            GovernanceVoteChoice::YES,
            100,
            12,
            "governance-v1",
            "gov-prop-001",
            "forged-proof"
        )
    );
    assert(!evidence.isValid());
}

void testAbstainRejectedWhenPolicyDisallowsIt() {
    const GovernanceVotingPolicy policy(
        "governance-v1",
        Amount::fromRawUnits(100),
        6000,
        Amount::fromRawUnits(1),
        false,
        false
    );
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope(),
        policy,
        makeRecord(GovernanceVoteChoice::ABSTAIN, 100, 12)
    );
    assert(!evidence.isValid());
}

void testBelowMinimumVotingPowerRejected() {
    const GovernanceVotingPolicy policy(
        "governance-v1",
        Amount::fromRawUnits(100),
        6000,
        Amount::fromRawUnits(50),
        true,
        false
    );
    const GovernanceVoteEvidence evidence(
        "evidence-001",
        validEnvelope(),
        policy,
        makeRecord(GovernanceVoteChoice::YES, 10, 12)
    );
    assert(!evidence.isValid());
}

} // namespace

int main() {
    testValidVoteEvidenceAccepted();
    testMissingEvidenceIdRejected();
    testVoteForDifferentProposalRejected();
    testPolicyVersionMismatchRejected();
    testVoteBeforeProposalSubmissionRejected();
    testForgedVoteProofRejected();
    testAbstainRejectedWhenPolicyDisallowsIt();
    testBelowMinimumVotingPowerRejected();
    return 0;
}
