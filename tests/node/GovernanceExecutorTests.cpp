#include "node/GovernanceExecutor.hpp"
#include "core/TransactionPayload.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo::node;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testProposalRequiresWeightedApprovalBeforeExecution() {
    GovernanceExecutor executor;
    std::string reason;
    const std::string payload =
        nodo::core::GovernanceProposalPayload::parameterChange(
            "Minimum fee",
            "Set minimum fee after voting closes",
            "MINIMUM_FEE_RAW",
            "500",
            10,
            1,
            2
        ).serialize();
    require(executor.submitProposal(
        "proposal-1", "owner-1", payload, 5, 1000, 100, reason), reason);
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW).empty(),
        "Submitting a proposal must not execute it.");

    require(executor.castVote(
        "proposal-1", "validator-a", nodo::core::GovernanceVoteChoice::YES,
        60, 6, 1001, "tx-vote-a", reason), reason);
    require(!executor.proposalApproved("proposal-1"),
        "A vote below two-thirds must not approve the proposal.");
    require(executor.castVote(
        "proposal-1", "validator-b", nodo::core::GovernanceVoteChoice::YES,
        10, 7, 1001, "tx-vote-b", reason), reason);
    require(!executor.proposalApproved("proposal-1"),
        "Votes must not decide the proposal before the voting period closes.");
    require(executor.advanceToHeight(8, 1002) == 0,
        "Decision at voting close should queue the not-yet-effective parameter change.");
    require(executor.proposalApproved("proposal-1"),
        "At least two-thirds of eligible weight must approve the proposal.");
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW).empty(),
        "Approved change must remain pending until its effective height.");

    require(executor.advanceToHeight(10, 1003) == 1,
        "Pending approved change must activate at its effective height.");
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW) == "500",
        "Approved change must become effective deterministically.");
}

void testDuplicateVoteAndInvalidPayloadAreRejected() {
    GovernanceExecutor executor;
    std::string reason;
    require(!executor.submitProposal(
        "bad", "owner",
        nodo::core::GovernanceProposalPayload::parameterChange(
            "Bad target", "Invalid target", "UNKNOWN", "1", 1, 0, 1
        ).serialize(),
        1, 1000, 2, reason), "Unknown governance target must be rejected.");
    require(executor.submitProposal(
        "proposal-2", "owner",
        nodo::core::GovernanceProposalPayload::parameterChange(
            "Minimum validators", "Set validator count", "MINIMUM_VALIDATOR_COUNT", "4", 2, 0, 2
        ).serialize(),
        1, 1000, 2, reason), reason);
    require(executor.castVote(
        "proposal-2", "validator-a", nodo::core::GovernanceVoteChoice::YES,
        1, 1, 1001, "tx-1", reason), reason);
    require(!executor.castVote(
        "proposal-2", "validator-a", nodo::core::GovernanceVoteChoice::YES,
        1, 1, 1001, "tx-2", reason),
        "Duplicate validator vote must be rejected.");
}

void testSerializationCommitsProposalAndVoteContents() {
    GovernanceExecutor left;
    GovernanceExecutor right;
    std::string reason;
    require(left.submitProposal(
        "proposal-a", "owner",
        nodo::core::GovernanceProposalPayload::parameterChange(
            "Fee A", "Set fee", "MINIMUM_FEE_RAW", "5", 5, 0, 1
        ).serialize(),
        1, 1000, 1, reason), reason);
    require(right.submitProposal(
        "proposal-b", "owner",
        nodo::core::GovernanceProposalPayload::parameterChange(
            "Fee B", "Set fee", "MINIMUM_FEE_RAW", "5", 5, 0, 1
        ).serialize(),
        1, 1000, 1, reason), reason);
    require(left.serialize() != right.serialize(),
        "Governance commitment must include proposal identity and payload, not only counts.");
}

} // namespace

int main() {
    try {
        testProposalRequiresWeightedApprovalBeforeExecution();
        testDuplicateVoteAndInvalidPayloadAreRejected();
        testSerializationCommitsProposalAndVoteContents();
        std::cout << "Governance executor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Governance executor tests failed: " << error.what() << '\n';
        return 1;
    }
}
