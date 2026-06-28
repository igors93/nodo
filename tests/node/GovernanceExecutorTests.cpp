#include "node/GovernanceExecutor.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::GovernanceExecutionStatus;
using nodo::node::GovernanceExecutor;
using nodo::node::GovernanceParameterTarget;

constexpr std::int64_t kTimestamp = 1900000000LL;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testExecuteProposalAppliedWhenHeightSufficient() {
    GovernanceExecutor executor;

    // effectiveHeight=100, currentHeight=100 → should be applied
    const auto result = executor.executeProposal(
        "proposal-001",
        "target=EPOCH_DURATION_SECONDS;value=7200;effectiveHeight=100",
        100,
        kTimestamp
    );

    requireCondition(
        result.isApplied(),
        "executeProposal should return APPLIED when currentHeight >= effectiveAtHeight."
    );

    requireCondition(
        result.status() == GovernanceExecutionStatus::APPLIED,
        "Status should be APPLIED."
    );

    requireCondition(
        result.change().newValue() == "7200",
        "Applied change should carry the new value '7200'."
    );
}

void testExecuteProposalPendingWhenHeightInsufficient() {
    GovernanceExecutor executor;

    // effectiveHeight=200, currentHeight=100 → should be pending
    const auto result = executor.executeProposal(
        "proposal-002",
        "target=MINIMUM_VALIDATOR_COUNT;value=5;effectiveHeight=200",
        100,
        kTimestamp
    );

    requireCondition(
        result.isPending(),
        "executeProposal should return PENDING when currentHeight < effectiveAtHeight."
    );

    requireCondition(
        result.status() == GovernanceExecutionStatus::PENDING,
        "Status should be PENDING."
    );
}

void testDoubleExecutionRejected() {
    GovernanceExecutor executor;

    // First execution — should succeed
    const auto first = executor.executeProposal(
        "proposal-003",
        "target=MAX_TRANSACTIONS_PER_BLOCK;value=1000;effectiveHeight=50",
        50,
        kTimestamp
    );

    requireCondition(
        first.isApplied(),
        "First execution of proposal-003 should be applied."
    );

    requireCondition(
        executor.hasBeenExecuted("proposal-003"),
        "hasBeenExecuted should return true after first execution."
    );

    // Second execution — should be rejected
    const auto second = executor.executeProposal(
        "proposal-003",
        "target=MAX_TRANSACTIONS_PER_BLOCK;value=1000;effectiveHeight=50",
        50,
        kTimestamp + 1
    );

    requireCondition(
        !second.isApplied(),
        "Second execution of the same proposal should not be applied."
    );

    requireCondition(
        second.status() == GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
        "Double execution should be REJECTED_INVALID_VALUE."
    );
}

void testRejectedForUnknownTarget() {
    GovernanceExecutor executor;

    const auto result = executor.executeProposal(
        "proposal-004",
        "target=UNKNOWN_TARGET_XYZ;value=42;effectiveHeight=10",
        10,
        kTimestamp
    );

    requireCondition(
        result.status() == GovernanceExecutionStatus::REJECTED_UNKNOWN_TARGET,
        "Proposal with unknown target should be REJECTED_UNKNOWN_TARGET."
    );
}

void testCurrentValueForTargetReturnsLastApplied() {
    GovernanceExecutor executor;

    // Apply first change
    executor.executeProposal(
        "proposal-005a",
        "target=MINIMUM_FEE_RAW;value=100;effectiveHeight=10",
        10,
        kTimestamp
    );

    requireCondition(
        executor.currentValueForTarget(GovernanceParameterTarget::MINIMUM_FEE_RAW) == "100",
        "currentValueForTarget should return '100' after first application."
    );

    // Apply second change (overrides first)
    executor.executeProposal(
        "proposal-005b",
        "target=MINIMUM_FEE_RAW;value=500;effectiveHeight=20",
        20,
        kTimestamp + 100
    );

    requireCondition(
        executor.currentValueForTarget(GovernanceParameterTarget::MINIMUM_FEE_RAW) == "500",
        "currentValueForTarget should return '500' after second application."
    );

    // Target with no applied change should return empty
    requireCondition(
        executor.currentValueForTarget(GovernanceParameterTarget::EPOCH_DURATION_SECONDS).empty(),
        "currentValueForTarget should return empty for a target with no applied changes."
    );
}

void testAppliedChangesAreOrderedChronologically() {
    GovernanceExecutor executor;

    executor.executeProposal(
        "proposal-chron-1",
        "target=MINIMUM_VALIDATOR_COUNT;value=3;effectiveHeight=10",
        10,
        kTimestamp
    );

    executor.executeProposal(
        "proposal-chron-2",
        "target=MAX_TRANSACTIONS_PER_BLOCK;value=500;effectiveHeight=11",
        11,
        kTimestamp + 60
    );

    executor.executeProposal(
        "proposal-chron-3",
        "target=VALIDATOR_REWARD_BASIS_POINTS;value=200;effectiveHeight=12",
        12,
        kTimestamp + 120
    );

    const auto& applied = executor.appliedChanges();

    requireCondition(
        applied.size() == 3,
        "Should have 3 applied changes."
    );

    requireCondition(
        applied[0].proposalId() == "proposal-chron-1" &&
        applied[1].proposalId() == "proposal-chron-2" &&
        applied[2].proposalId() == "proposal-chron-3",
        "Applied changes should be in chronological (insertion) order."
    );

    requireCondition(
        applied[0].appliedAt() < applied[1].appliedAt() &&
        applied[1].appliedAt() < applied[2].appliedAt(),
        "Applied timestamps should be strictly increasing."
    );
}

void testPendingProposalActivatesAtHeightBoundary() {
    GovernanceExecutor executor;
    const auto pending = executor.executeProposal(
        "proposal-pending-boundary",
        "target=MINIMUM_FEE_RAW,value=750,effectiveHeight=25",
        24,
        kTimestamp
    );
    requireCondition(
        pending.isPending() && executor.pendingChanges().size() == 1,
        "A future comma-delimited proposal must remain pending."
    );
    requireCondition(
        executor.advanceToHeight(24, kTimestamp + 1) == 0,
        "A pending proposal must not activate before its height."
    );
    requireCondition(
        executor.advanceToHeight(25, kTimestamp + 2) == 1 &&
        executor.currentValueForTarget(
            GovernanceParameterTarget::MINIMUM_FEE_RAW
        ) == "750" &&
        executor.pendingChanges().empty(),
        "A pending proposal must activate exactly at its height boundary."
    );
}

void testProposalRequiresExplicitActivationHeight() {
    GovernanceExecutor executor;
    const auto result = executor.executeProposal(
        "proposal-without-height",
        "target=MINIMUM_FEE_RAW,value=100",
        1,
        kTimestamp
    );
    requireCondition(
        result.status() == GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
        "A governance proposal without a positive activation height must be rejected."
    );
}

} // namespace

int main() {
    try {
        testExecuteProposalAppliedWhenHeightSufficient();
        testExecuteProposalPendingWhenHeightInsufficient();
        testDoubleExecutionRejected();
        testRejectedForUnknownTarget();
        testCurrentValueForTargetReturnsLastApplied();
        testAppliedChangesAreOrderedChronologically();
        testPendingProposalActivatesAtHeightBoundary();
        testProposalRequiresExplicitActivationHeight();

        std::cout << "Nodo GovernanceExecutor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo GovernanceExecutor tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
