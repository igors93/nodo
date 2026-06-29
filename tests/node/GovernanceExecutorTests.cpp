#include "node/GovernanceExecutor.hpp"

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
        "target=MINIMUM_FEE_RAW,value=500,effectiveHeight=10";
    require(executor.submitProposal(
        "proposal-1", "owner-1", payload, 5, 1000, reason), reason);
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW).empty(),
        "Submitting a proposal must not execute it.");

    require(executor.castVote(
        "proposal-1", "validator-a", true, 60, 100, 6, 1001, reason), reason);
    require(!executor.proposalApproved("proposal-1"),
        "A vote below two-thirds must not approve the proposal.");
    require(executor.castVote(
        "proposal-1", "validator-b", true, 10, 100, 6, 1001, reason), reason);
    require(executor.proposalApproved("proposal-1"),
        "At least two-thirds of eligible weight must approve the proposal.");
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW).empty(),
        "Approved change must remain pending until its effective height.");

    require(executor.advanceToHeight(10, 1002) == 1,
        "Pending approved change must activate at its effective height.");
    require(executor.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW) == "500",
        "Approved change must become effective deterministically.");
}

void testDuplicateVoteAndInvalidPayloadAreRejected() {
    GovernanceExecutor executor;
    std::string reason;
    require(!executor.submitProposal(
        "bad", "owner", "target=UNKNOWN,value=1,effectiveHeight=1",
        1, 1000, reason), "Unknown governance target must be rejected.");
    require(executor.submitProposal(
        "proposal-2", "owner", "target=MINIMUM_VALIDATOR_COUNT,value=4,effectiveHeight=2",
        1, 1000, reason), reason);
    require(executor.castVote(
        "proposal-2", "validator-a", true, 1, 2, 1, 1001, reason), reason);
    require(!executor.castVote(
        "proposal-2", "validator-a", true, 1, 2, 1, 1001, reason),
        "Duplicate validator vote must be rejected.");
}

void testSerializationCommitsProposalAndVoteContents() {
    GovernanceExecutor left;
    GovernanceExecutor right;
    std::string reason;
    require(left.submitProposal(
        "proposal-a", "owner", "target=MINIMUM_FEE_RAW,value=5,effectiveHeight=5",
        1, 1000, reason), reason);
    require(right.submitProposal(
        "proposal-b", "owner", "target=MINIMUM_FEE_RAW,value=5,effectiveHeight=5",
        1, 1000, reason), reason);
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
