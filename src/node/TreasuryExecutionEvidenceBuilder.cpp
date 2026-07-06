#include "node/TreasuryExecutionEvidenceBuilder.hpp"

#include "economics/DefenseModeState.hpp"
#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/TreasurySpendValidator.hpp"
#include "node/GovernanceLifecycleRecordBuilder.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/ValidatorLifecycle.hpp"

namespace nodo::node {

namespace {

economics::TreasuryPolicy
treasuryPolicyFrom(const config::NetworkParameters &networkParameters) {
  return economics::TreasuryPolicy(
      "nodo-treasury-policy-v1",
      utils::Amount::fromRawUnits(static_cast<std::int64_t>(
          networkParameters.treasuryMaxSpendPerEpochRawUnits())),
      utils::Amount::fromRawUnits(static_cast<std::int64_t>(
          networkParameters.treasuryMaxSpendPerProposalRawUnits())),
      networkParameters.treasuryTimelockBlocks(), true, false);
}

} // namespace

std::map<std::string, GovernanceProposalStatus>
TreasuryExecutionEvidenceBuilder::snapshotStatuses(
    const GovernanceExecutor &governance) {
  std::map<std::string, GovernanceProposalStatus> statuses;
  for (const auto &proposalId : governance.proposalIds()) {
    statuses[proposalId] = governance.proposalStatus(proposalId);
  }
  return statuses;
}

std::vector<economics::TreasuryExecutionEvidence>
TreasuryExecutionEvidenceBuilder::buildForNewlyExecuted(
    const std::map<std::string, GovernanceProposalStatus> &statusesBefore,
    const GovernanceExecutor &governanceAfter,
    const config::NetworkParameters &networkParameters) {
  std::vector<economics::TreasuryExecutionEvidence> evidence;

  for (const auto &proposalId : governanceAfter.proposalIds()) {
    if (governanceAfter.proposalStatus(proposalId) !=
        GovernanceProposalStatus::EXECUTED) {
      continue;
    }
    const auto beforeIt = statusesBefore.find(proposalId);
    if (beforeIt != statusesBefore.end() &&
        beforeIt->second == GovernanceProposalStatus::EXECUTED) {
      continue; // executed in a previous block, not this one
    }

    const GovernanceExecutor::GovernanceProposalSnapshot snapshot =
        governanceAfter.proposalSnapshot(proposalId);

    const auto decided =
        GovernanceLifecycleRecordBuilder::buildDecided(snapshot);
    if (!decided.has_value()) {
      continue;
    }

    const economics::GovernanceApprovalBridgeResult bridgeResult =
        economics::GovernanceApprovalBridge::
            produceTreasuryApprovalFromVerifiedLifecycle(*decided);
    if (!bridgeResult.isAccepted()) {
      continue;
    }

    const std::uint64_t currentEpoch =
        ValidatorLifecycle::epochIndexForBlock(snapshot.executedAtHeight);

    const economics::TreasuryProposal treasuryProposal(
        proposalId, snapshot.payload.treasuryRecipient(),
        utils::Amount::fromRawUnits(snapshot.payload.treasuryAmountRaw()),
        snapshot.payload.title(), snapshot.decidedAtHeight, currentEpoch,
        snapshot.proposerAddress);

    const economics::TreasuryAccount treasuryBefore(
        "treasury-main", ProtectionTreasury::TREASURY_ADDRESS,
        snapshot.treasuryBalanceBeforeExecution, currentEpoch, false,
        "unlocked");

    const economics::TreasuryPolicy policy =
        treasuryPolicyFrom(networkParameters);
    const utils::Amount epochSpentSoFar =
        governanceAfter.treasuryEpochSpentSoFar(snapshot.executedAtHeight,
                                                proposalId);

    const economics::TreasurySpendValidationResult spendResult =
        economics::TreasurySpendValidator::validateSpend(
            economics::DefenseModeState::INACTIVE,
            economics::DefenseModePolicy::defaultPolicy(), treasuryBefore,
            policy, treasuryProposal, bridgeResult.treasuryApproval(),
            snapshot.executedAtHeight, epochSpentSoFar);
    if (!spendResult.accepted()) {
      continue;
    }

    economics::GovernanceApprovalContext context{*decided};
    evidence.emplace_back("evidence-" + proposalId, treasuryProposal,
                          bridgeResult.treasuryApproval(), policy,
                          treasuryBefore, snapshot.executedAtHeight,
                          epochSpentSoFar, spendResult.spendRecord(),
                          snapshot.executedAt, std::move(context));
  }

  return evidence;
}

} // namespace nodo::node
