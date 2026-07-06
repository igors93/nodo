#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/TransactionPayload.hpp"
#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceLifecycleVerifier.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/GovernanceLifecycleRecordBuilder.hpp"
#include "node/ProtectionTreasury.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo::node;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

void testProposalRequiresWeightedApprovalBeforeExecution() {
  GovernanceExecutor executor;
  std::string reason;
  const std::string payload =
      nodo::core::GovernanceProposalPayload::parameterChange(
          "Minimum fee", "Set minimum fee after voting closes",
          "MINIMUM_FEE_RAW", "500", 10, 1, 2)
          .serialize();
  require(executor.submitProposal("proposal-1", "owner-1", payload, 5, 1000,
                                  100, reason),
          reason);
  require(
      executor.currentValueForTarget(GovernanceParameterTarget::MINIMUM_FEE_RAW)
          .empty(),
      "Submitting a proposal must not execute it.");

  require(executor.castVote("proposal-1", "validator-a",
                            nodo::core::GovernanceVoteChoice::YES, 60, 6, 1001,
                            "tx-vote-a", reason),
          reason);
  require(!executor.proposalApproved("proposal-1"),
          "A vote below two-thirds must not approve the proposal.");
  require(executor.castVote("proposal-1", "validator-b",
                            nodo::core::GovernanceVoteChoice::YES, 10, 7, 1001,
                            "tx-vote-b", reason),
          reason);
  require(
      !executor.proposalApproved("proposal-1"),
      "Votes must not decide the proposal before the voting period closes.");
  require(executor.advanceToHeight(8, 1002) == 0,
          "Decision at voting close should queue the not-yet-effective "
          "parameter change.");
  require(executor.proposalApproved("proposal-1"),
          "At least two-thirds of eligible weight must approve the proposal.");
  require(
      executor.currentValueForTarget(GovernanceParameterTarget::MINIMUM_FEE_RAW)
          .empty(),
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
                  "Bad target", "Invalid target", "UNKNOWN", "1", 1, 0, 1)
                  .serialize(),
              1, 1000, 2, reason),
          "Unknown governance target must be rejected.");
  require(executor.submitProposal(
              "proposal-2", "owner",
              nodo::core::GovernanceProposalPayload::parameterChange(
                  "Minimum validators", "Set validator count",
                  "MINIMUM_VALIDATOR_COUNT", "4", 2, 0, 2)
                  .serialize(),
              1, 1000, 2, reason),
          reason);
  require(executor.castVote("proposal-2", "validator-a",
                            nodo::core::GovernanceVoteChoice::YES, 1, 1, 1001,
                            "tx-1", reason),
          reason);
  require(!executor.castVote("proposal-2", "validator-a",
                             nodo::core::GovernanceVoteChoice::YES, 1, 1, 1001,
                             "tx-2", reason),
          "Duplicate validator vote must be rejected.");
}

void testSerializationCommitsProposalAndVoteContents() {
  GovernanceExecutor left;
  GovernanceExecutor right;
  std::string reason;
  require(left.submitProposal(
              "proposal-a", "owner",
              nodo::core::GovernanceProposalPayload::parameterChange(
                  "Fee A", "Set fee", "MINIMUM_FEE_RAW", "5", 5, 0, 1)
                  .serialize(),
              1, 1000, 1, reason),
          reason);
  require(right.submitProposal(
              "proposal-b", "owner",
              nodo::core::GovernanceProposalPayload::parameterChange(
                  "Fee B", "Set fee", "MINIMUM_FEE_RAW", "5", 5, 0, 1)
                  .serialize(),
              1, 1000, 1, reason),
          reason);
  require(left.serialize() != right.serialize(),
          "Governance commitment must include proposal identity and payload, "
          "not only counts.");
}

void testTreasurySpendIsQueuedTimelockedAndVerifiable() {
  GovernanceExecutor executor;
  nodo::core::AccountStateView accounts;
  require(accounts.putAccount(nodo::core::AccountState(
              ProtectionTreasury::TREASURY_ADDRESS,
              nodo::utils::Amount::fromRawUnits(1000000), 0)),
          "Failed to seed treasury account.");

  std::string reason;
  const std::string payload =
      nodo::core::GovernanceProposalPayload::treasurySpend(
          "Fund grants", "Pay recipient-1", "recipient-1", 10000,
          /*votingStartDelayBlocks=*/1, /*votingPeriodBlocks=*/5)
          .serialize();

  require(executor.submitProposal("treasury-1", "proposer-1", payload, 1, 1000,
                                  3000, reason),
          reason);

  require(executor.castVote("treasury-1", "validator-a",
                            nodo::core::GovernanceVoteChoice::YES, 1000, 2,
                            1001, "tx-a", reason),
          reason);
  require(executor.castVote("treasury-1", "validator-b",
                            nodo::core::GovernanceVoteChoice::YES, 1000, 2,
                            1001, "tx-b", reason),
          reason);
  require(executor.castVote("treasury-1", "validator-c",
                            nodo::core::GovernanceVoteChoice::NO, 1000, 2, 1001,
                            "tx-c", reason),
          reason);

  // Voting starts at height 2 (createdHeight 1 + delay 1), period 5 blocks
  // -> ends at height 6. advanceToHeight(7, ...) is the first height that
  // closes voting.
  constexpr std::uint64_t treasuryTimelockBlocks = 10;
  require(
      executor.advanceToHeight(7, 1002, &accounts, treasuryTimelockBlocks) == 0,
      "Approving a treasury spend must queue it for execution, not execute it "
      "immediately.");
  require(executor.proposalStatus("treasury-1") ==
              GovernanceProposalStatus::QUEUED_FOR_EXECUTION,
          "A 2/3-approved treasury spend must be queued for execution behind a "
          "timelock.");
  require(!executor.proposalReadyForExplicitExecution("treasury-1", 7),
          "Treasury spend must not be executable before its timelock elapses.");

  const nodo::economics::TreasuryPolicy policy(
      "test-treasury-policy", nodo::utils::Amount::fromRawUnits(1000000),
      nodo::utils::Amount::fromRawUnits(1000000), treasuryTimelockBlocks, true,
      false);

  const GovernanceExecutor::GovernanceProposalSnapshot decidedSnapshot =
      executor.proposalSnapshot("treasury-1");
  const std::optional<nodo::economics::GovernanceLifecycleRecord> decided =
      GovernanceLifecycleRecordBuilder::buildDecided(decidedSnapshot);
  require(decided.has_value(),
          "Decided treasury proposal must produce a lifecycle record.");
  require(
      nodo::economics::GovernanceLifecycleVerifier::verify(*decided).verified(),
      "Decided lifecycle record must independently verify.");

  const nodo::economics::GovernanceApprovalBridgeResult bridgeResult =
      nodo::economics::GovernanceApprovalBridge::
          produceTreasuryApprovalFromVerifiedLifecycle(*decided);
  require(bridgeResult.isAccepted(),
          "Governance approval bridge rejected a verified lifecycle: " +
              bridgeResult.reason());

  const GovernanceExecutor::TreasuryExecutionOutcome earlyAttempt =
      executor.executeQueuedTreasuryProposal("treasury-1", 7, 1002, policy,
                                             bridgeResult.treasuryApproval(),
                                             accounts);
  require(!earlyAttempt.applied,
          "Execution before the timelock elapses must be rejected.");

  const std::uint64_t executableHeight = 7 + treasuryTimelockBlocks;
  const GovernanceExecutor::TreasuryExecutionOutcome outcome =
      executor.executeQueuedTreasuryProposal(
          "treasury-1", executableHeight, 1500, policy,
          bridgeResult.treasuryApproval(), accounts);
  require(outcome.applied,
          "Treasury execution after the timelock must succeed: " +
              outcome.reason);
  require(executor.proposalStatus("treasury-1") ==
              GovernanceProposalStatus::EXECUTED,
          "Executed treasury spend must reach EXECUTED status.");
  require(accounts.accountOrDefault(ProtectionTreasury::TREASURY_ADDRESS)
                  .balance()
                  .rawUnits() == 990000,
          "Treasury balance must be decremented by the spend amount.");
  require(accounts.accountOrDefault("recipient-1").balance().rawUnits() ==
              10000,
          "Recipient balance must be credited by the spend amount.");

  const GovernanceExecutor::GovernanceProposalSnapshot executedSnapshot =
      executor.proposalSnapshot("treasury-1");
  const nodo::economics::GovernanceLifecycleRecord executedLifecycle =
      GovernanceLifecycleRecordBuilder::buildExecuted(
          *decided, executedSnapshot.executedAtHeight);
  require(
      nodo::economics::GovernanceLifecycleVerifier::verify(executedLifecycle)
          .verified(),
      "Executed lifecycle record must independently verify.");
}

} // namespace

int main() {
  try {
    testProposalRequiresWeightedApprovalBeforeExecution();
    testDuplicateVoteAndInvalidPayloadAreRejected();
    testSerializationCommitsProposalAndVoteContents();
    testTreasurySpendIsQueuedTimelockedAndVerifiable();
    std::cout << "Governance executor tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Governance executor tests failed: " << error.what() << '\n';
    return 1;
  }
}
