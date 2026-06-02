#ifndef NODO_TESTS_COMMON_GOVERNANCE_LIFECYCLE_FIXTURES_HPP
#define NODO_TESTS_COMMON_GOVERNANCE_LIFECYCLE_FIXTURES_HPP

#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceLifecycleRecord.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace nodo::tests::fixtures {

inline economics::TreasuryAccount validTreasury(
    utils::Amount balance = utils::Amount::fromRawUnits(1000000)
) {
    return economics::TreasuryAccount(
        "treasury-main",
        "nodo-treasury-addr",
        balance,
        0,
        false,
        "unlocked"
    );
}

inline economics::TreasuryPolicy validSpendPolicy(
    std::uint64_t timelock = 5
) {
    return economics::TreasuryPolicy(
        "treasury-policy-v1",
        utils::Amount::fromRawUnits(500000),
        utils::Amount::fromRawUnits(100000),
        timelock,
        true,
        false
    );
}

inline economics::TreasuryProposal validTreasuryProposal(
    const std::string& proposalId = "prop-001"
) {
    return economics::TreasuryProposal(
        proposalId,
        "recipient-addr",
        utils::Amount::fromRawUnits(50000),
        "fund-validator",
        1,
        0,
        "proposer-node"
    );
}

inline economics::GovernancePolicy validGovernancePolicy() {
    return economics::GovernancePolicy("governance-v1", 10, 5, true, false);
}

inline economics::GovernanceVotingPolicy validVotingPolicy() {
    return economics::GovernanceVotingPolicy("governance-v1", 100, 60, true, true);
}

inline economics::GovernanceProposalEnvelope validEnvelope(
    const std::string& proposalId = "prop-001",
    const std::string& governanceProposalId = "gov-prop-001"
) {
    return economics::GovernanceProposalEnvelope(
        governanceProposalId,
        "TREASURY_SPEND",
        validTreasuryProposal(proposalId),
        5,
        "submitter-node",
        "governance-v1",
        "hash-abc123"
    );
}

inline economics::GovernanceVoteEvidence voteEvidence(
    const std::string& voteId,
    const std::string& voterId,
    economics::GovernanceVoteChoice choice,
    std::uint64_t votingPower,
    const std::string& governanceProposalId = "gov-prop-001",
    std::uint64_t votedAtBlock = 12
) {
    economics::GovernanceVoteRecord record(
        voteId,
        governanceProposalId,
        voterId,
        choice,
        votingPower,
        votedAtBlock,
        "governance-v1"
    );

    return economics::GovernanceVoteEvidence(
        record,
        economics::GovernanceVoteProof::build(record)
    );
}

inline std::vector<economics::GovernanceVoteEvidence> validVotes(
    const std::string& governanceProposalId = "gov-prop-001"
) {
    return {
        voteEvidence(
            "vote-001",
            "validator-a",
            economics::GovernanceVoteChoice::YES,
            60,
            governanceProposalId
        ),
        voteEvidence(
            "vote-002",
            "validator-b",
            economics::GovernanceVoteChoice::NO,
            20,
            governanceProposalId
        ),
        voteEvidence(
            "vote-003",
            "validator-c",
            economics::GovernanceVoteChoice::ABSTAIN,
            20,
            governanceProposalId
        )
    };
}

inline economics::GovernanceLifecycleRecord validLifecycle(
    const std::string& lifecycleId = "lifecycle-001",
    const std::string& proposalId = "prop-001",
    const std::string& governanceProposalId = "gov-prop-001",
    std::uint64_t decidedAtBlock = 20
) {
    const economics::GovernancePolicy governancePolicy =
        validGovernancePolicy();
    const economics::GovernanceVotingPolicy votingPolicy =
        validVotingPolicy();
    const economics::GovernanceProposalEnvelope envelope =
        validEnvelope(proposalId, governanceProposalId);
    std::vector<economics::GovernanceVoteEvidence> votes =
        validVotes(governanceProposalId);

    const economics::GovernanceVoteSetAuditResult voteAudit =
        economics::GovernanceVoteSetAudit::auditVotes(
            votingPolicy,
            governanceProposalId,
            votes
        );
    assert(voteAudit.accepted());

    const economics::GovernanceDecisionBuildResult decisionBuild =
        economics::GovernanceDecisionBuilder::buildDecision(
            envelope,
            votingPolicy,
            voteAudit.tallyReport(),
            decidedAtBlock,
            "governance-node"
        );
    assert(decisionBuild.built());

    economics::GovernanceLifecycleRecord lifecycle(
        lifecycleId,
        envelope,
        governancePolicy,
        votingPolicy,
        std::move(votes),
        voteAudit.tallyReport(),
        decisionBuild.decisionRecord(),
        5,
        decidedAtBlock
    );
    assert(lifecycle.isValid());
    return lifecycle;
}

inline economics::TreasuryApproval approvalFromLifecycle(
    const economics::GovernanceLifecycleRecord& lifecycle
) {
    const economics::GovernanceApprovalBridgeResult bridgeResult =
        economics::GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle(
            lifecycle
        );
    assert(bridgeResult.isAccepted());
    return bridgeResult.treasuryApproval();
}

inline economics::TreasuryExecutionEvidence validExecutionEvidence(
    const std::string& evidenceId = "ev-001",
    const std::string& lifecycleId = "lifecycle-001",
    const std::string& proposalId = "prop-001",
    const std::string& governanceProposalId = "gov-prop-001",
    std::uint64_t currentBlockHeight = 10
) {
    const economics::TreasuryProposal proposal =
        validTreasuryProposal(proposalId);
    const economics::TreasuryPolicy spendPolicy =
        validSpendPolicy();
    const economics::TreasuryAccount treasury =
        validTreasury();
    const economics::GovernanceLifecycleRecord lifecycle =
        validLifecycle(lifecycleId, proposalId, governanceProposalId);
    const economics::TreasuryApproval approval =
        approvalFromLifecycle(lifecycle);

    const economics::TreasurySpendValidationResult spendResult =
        economics::TreasurySpendValidator::validateSpend(
            treasury,
            spendPolicy,
            proposal,
            approval,
            currentBlockHeight,
            utils::Amount::fromRawUnits(0)
        );
    assert(spendResult.accepted());

    economics::GovernanceApprovalContext context;
    context.governanceLifecycle = lifecycle;

    return economics::TreasuryExecutionEvidence(
        evidenceId,
        proposal,
        approval,
        spendPolicy,
        treasury,
        currentBlockHeight,
        utils::Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(context)
    );
}

} // namespace nodo::tests::fixtures

#endif
