#ifndef NODO_NODE_GOVERNANCE_EXECUTOR_HPP
#define NODO_NODE_GOVERNANCE_EXECUTOR_HPP

#include "core/AccountStateView.hpp"
#include "core/TransactionPayload.hpp"
#include "economics/TreasurySpendValidator.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * GovernanceParameterTarget identifies which protocol parameter a governance
 * proposal is requesting to change.
 */
enum class GovernanceParameterTarget {
  EPOCH_DURATION_SECONDS,
  MINIMUM_VALIDATOR_COUNT,
  QUORUM_THRESHOLD_NUMERATOR,
  QUORUM_THRESHOLD_DENOMINATOR,
  MAX_TRANSACTIONS_PER_BLOCK,
  MINIMUM_FEE_RAW,
  TREASURY_ALLOCATION_BASIS_POINTS,
  VALIDATOR_REWARD_BASIS_POINTS,
  UNKNOWN
};

std::string governanceParameterTargetToString(GovernanceParameterTarget target);
GovernanceParameterTarget
governanceParameterTargetFromString(const std::string &s);

/*
 * GovernanceParameterChange is an immutable audit record of one applied or
 * pending parameter change. Every applied change stores both the previous and
 * new values so any node can independently verify the parameter history.
 *
 * Security principle:
 * Parameter changes must be deterministic and auditable. The proposalId ties
 * each change to the governance vote that authorized it.
 */
class GovernanceParameterChange {
public:
  GovernanceParameterChange();

  GovernanceParameterChange(std::string proposalId,
                            GovernanceParameterTarget target,
                            std::string previousValue, std::string newValue,
                            std::uint64_t effectiveAtHeight,
                            std::int64_t appliedAt);

  static GovernanceParameterChange pending(std::string proposalId,
                                           GovernanceParameterTarget target,
                                           std::string newValue,
                                           std::uint64_t effectiveAtHeight);

  const std::string &proposalId() const;
  GovernanceParameterTarget target() const;
  const std::string &previousValue() const;
  const std::string &newValue() const;
  std::uint64_t effectiveAtHeight() const;
  std::int64_t appliedAt() const;

  bool isApplied() const;
  bool isValid() const;
  std::string serialize() const;
  static GovernanceParameterChange deserialize(const std::string &s);

private:
  std::string m_proposalId;
  GovernanceParameterTarget m_target;
  std::string m_previousValue;
  std::string m_newValue;
  std::uint64_t m_effectiveAtHeight;
  std::int64_t m_appliedAt; // 0 = pending (not yet applied)
};

enum class GovernanceExecutionStatus {
  APPLIED,
  PENDING,
  REJECTED_UNKNOWN_TARGET,
  REJECTED_INVALID_VALUE,
  REJECTED_NOT_YET_EFFECTIVE
};

std::string governanceExecutionStatusToString(GovernanceExecutionStatus status);

enum class GovernanceProposalStatus {
  PENDING,
  ACTIVE,
  APPROVED,
  REJECTED,
  EXPIRED,
  QUEUED_FOR_EXECUTION,
  EXECUTED,
  FAILED_EXECUTION
};

std::string governanceProposalStatusToString(GovernanceProposalStatus status);

class GovernanceTallySnapshot {
public:
  GovernanceTallySnapshot();

  GovernanceTallySnapshot(std::string proposalId, std::uint64_t yesWeight,
                          std::uint64_t noWeight, std::uint64_t abstainWeight,
                          std::uint64_t participatingWeight,
                          std::uint64_t totalEligibleWeight, bool quorumMet,
                          bool approvalThresholdMet);

  const std::string &proposalId() const;
  std::uint64_t yesWeight() const;
  std::uint64_t noWeight() const;
  std::uint64_t abstainWeight() const;
  std::uint64_t participatingWeight() const;
  std::uint64_t totalEligibleWeight() const;
  bool quorumMet() const;
  bool approvalThresholdMet() const;
  bool approved() const;
  std::string serialize() const;

private:
  std::string m_proposalId;
  std::uint64_t m_yesWeight;
  std::uint64_t m_noWeight;
  std::uint64_t m_abstainWeight;
  std::uint64_t m_participatingWeight;
  std::uint64_t m_totalEligibleWeight;
  bool m_quorumMet;
  bool m_approvalThresholdMet;
};

class GovernanceExecutionResult {
public:
  static GovernanceExecutionResult applied(GovernanceParameterChange change);
  static GovernanceExecutionResult applied(std::string detail);
  static GovernanceExecutionResult pending(std::string proposalId,
                                           std::uint64_t effectiveAtHeight,
                                           std::uint64_t currentHeight);
  static GovernanceExecutionResult rejected(GovernanceExecutionStatus reason,
                                            std::string detail);

  GovernanceExecutionStatus status() const;
  const std::string &detail() const;
  bool isApplied() const;
  bool isPending() const;
  const GovernanceParameterChange &change() const;
  std::string serialize() const;

private:
  GovernanceExecutionStatus m_status;
  std::string m_detail;
  GovernanceParameterChange m_change;

  GovernanceExecutionResult(GovernanceExecutionStatus status,
                            std::string detail,
                            GovernanceParameterChange change);
};

/*
 * GovernanceExecutor applies approved governance decisions to live protocol
 * parameters. It is the single authority that transforms a DECIDED_APPROVED
 * governance lifecycle state into a concrete parameter change.
 *
 * Proposal payloads are canonical core::GovernanceProposalPayload values.
 *
 * Security principle:
 * Double-execution is prevented by tracking every executed proposalId.
 * Parameter changes are validated before application; unknown targets or
 * out-of-range values are rejected with an audit record.
 */
class GovernanceExecutor {
public:
  GovernanceExecutor();

  bool submitProposal(const std::string &proposalId,
                      const std::string &proposerAddress,
                      const std::string &proposalPayload,
                      std::uint64_t currentHeight, std::int64_t now,
                      std::uint64_t totalEligibleWeight, std::string &reason);

  bool castVote(const std::string &proposalId,
                const std::string &validatorAddress,
                core::GovernanceVoteChoice choice, std::uint64_t votingWeight,
                std::uint64_t currentHeight, std::int64_t now,
                const std::string &transactionId, std::string &reason);

  bool hasProposal(const std::string &proposalId) const;
  bool hasVote(const std::string &proposalId,
               const std::string &validatorAddress) const;
  bool proposalApproved(const std::string &proposalId) const;
  bool proposalOpenForVoting(const std::string &proposalId,
                             std::uint64_t height) const;
  std::uint64_t proposalVotingStartHeight(const std::string &proposalId) const;
  std::uint64_t proposalVotingEndHeight(const std::string &proposalId) const;
  GovernanceProposalStatus proposalStatus(const std::string &proposalId) const;
  GovernanceTallySnapshot tallyForProposal(const std::string &proposalId) const;
  std::string proposalDetail(const std::string &proposalId) const;
  std::string proposalVotes(const std::string &proposalId) const;
  std::string proposalExecutionDetail(const std::string &proposalId) const;

  /*
   * GovernanceVoteInfo / GovernanceProposalSnapshot expose a proposal's raw
   * data in structured form, so callers outside GovernanceExecutor (audit
   * evidence builders, treasury execution) do not need to string-parse
   * proposalDetail()/proposalVotes().
   */
  struct GovernanceVoteInfo {
    std::string validatorAddress;
    core::GovernanceVoteChoice choice = core::GovernanceVoteChoice::NO;
    std::uint64_t weight = 0;
    std::uint64_t castHeight = 0;
    std::int64_t castAt = 0;
    std::string transactionId;
  };

  struct GovernanceProposalSnapshot {
    std::string proposalId;
    std::string proposerAddress;
    core::GovernanceProposalPayload payload;
    std::uint64_t createdHeight = 0;
    std::int64_t createdAt = 0;
    std::uint64_t votingStartHeight = 0;
    std::uint64_t votingEndHeight = 0;
    std::uint64_t totalEligibleWeight = 0;
    GovernanceProposalStatus status = GovernanceProposalStatus::PENDING;
    GovernanceTallySnapshot tally;
    std::uint64_t decidedAtHeight = 0;
    std::int64_t decidedAt = 0;
    std::uint64_t executedAtHeight = 0;
    std::int64_t executedAt = 0;
    utils::Amount treasuryBalanceBeforeExecution;
    std::vector<GovernanceVoteInfo> votes;
  };

  // Returns a snapshot with an empty proposalId if the proposal is unknown.
  GovernanceProposalSnapshot
  proposalSnapshot(const std::string &proposalId) const;

  static bool validateProposalPayload(const std::string &proposalPayload,
                                      std::string &reason);

  // Activates every pending change whose effective height has been reached,
  // and closes voting on any proposal past its votingEndHeight. Treasury
  // spend proposals that are approved here are queued for execution (see
  // proposalReadyForExplicitExecution/executeQueuedTreasuryProposal) rather
  // than executed immediately: they require an explicit GOVERNANCE_EXECUTE
  // transaction once treasuryTimelockBlocks has elapsed, so anyone can
  // observe an approved spend before it moves funds.
  // Returns the number of changes activated in deterministic insertion order.
  std::size_t advanceToHeight(std::uint64_t currentHeight, std::int64_t now,
                              core::AccountStateView *accounts = nullptr,
                              std::uint64_t treasuryTimelockBlocks = 0);

  // Light eligibility check for admitting a GOVERNANCE_EXECUTE transaction:
  // is this a treasury-spend proposal, queued for execution, whose timelock
  // has elapsed? Full policy/limit/balance validation happens only inside
  // executeQueuedTreasuryProposal, which is the sole authority that mutates
  // account state.
  bool proposalReadyForExplicitExecution(const std::string &proposalId,
                                         std::uint64_t currentHeight) const;

  struct TreasuryExecutionOutcome {
    bool applied = false;
    std::string reason;
    economics::TreasurySpendRecord spendRecord;
    utils::Amount treasuryBalanceBefore;
  };

  // Authoritative treasury-spend execution, triggered by a GOVERNANCE_EXECUTE
  // transaction. `approval` must already have been produced by
  // economics::GovernanceApprovalBridge from a verified lifecycle record —
  // GovernanceExecutor does not build lifecycle/evidence records itself,
  // that is an audit-trail concern layered on top of this deterministic
  // state machine, not part of it.
  TreasuryExecutionOutcome
  executeQueuedTreasuryProposal(const std::string &proposalId,
                                std::uint64_t currentHeight, std::int64_t now,
                                const economics::TreasuryPolicy &treasuryPolicy,
                                const economics::TreasuryApproval &approval,
                                core::AccountStateView &accounts);

  // Sum of treasury amounts already EXECUTED within the same epoch as
  // currentHeight, excluding excludingProposalId (the proposal currently
  // being validated, if it happens to be counted among executed proposals).
  // Exposed publicly (in addition to internal use by
  // executeQueuedTreasuryProposal) so audit-trail evidence builders can
  // deterministically reproduce the epoch-spend figure a past execution saw,
  // without duplicating this scan.
  utils::Amount
  treasuryEpochSpentSoFar(std::uint64_t currentHeight,
                          const std::string &excludingProposalId) const;

  // Returns all applied changes in chronological order.
  const std::vector<GovernanceParameterChange> &appliedChanges() const;

  // Returns pending changes not yet effective.
  std::vector<GovernanceParameterChange> pendingChanges() const;

  // Check if a proposal has already been executed (prevents double-execution).
  bool hasBeenExecuted(const std::string &proposalId) const;

  // Get the current effective value for a parameter target.
  // Returns empty string if no governance change has been applied for this
  // target.
  std::string currentValueForTarget(GovernanceParameterTarget target) const;

  std::size_t activeProposalCount() const;
  std::size_t approvedProposalCount() const;
  std::size_t executableProposalCount(std::uint64_t currentHeight) const;
  std::size_t executedProposalCount() const;

  std::vector<std::string> proposalIds() const;

  std::string serialize() const;

private:
  struct VoteState {
    core::GovernanceVoteChoice choice = core::GovernanceVoteChoice::NO;
    std::uint64_t weight = 0;
    std::uint64_t castHeight = 0;
    std::int64_t castAt = 0;
    std::string transactionId;
  };
  struct ProposalState {
    std::string proposerAddress;
    core::GovernanceProposalPayload payload;
    std::uint64_t createdHeight = 0;
    std::int64_t createdAt = 0;
    std::uint64_t votingStartHeight = 0;
    std::uint64_t votingEndHeight = 0;
    std::uint64_t totalEligibleWeight = 0;
    GovernanceProposalStatus status = GovernanceProposalStatus::PENDING;
    GovernanceTallySnapshot finalTally;
    std::uint64_t decidedAtHeight = 0;
    std::int64_t decidedAt = 0;
    std::uint64_t executedAtHeight = 0;
    std::int64_t executedAt = 0;
    std::string executionDetail;
    std::map<std::string, VoteState> votes;

    // Treasury-spend-only: the height at which a QUEUED_FOR_EXECUTION
    // spend becomes eligible for an explicit GOVERNANCE_EXECUTE
    // transaction (decidedAtHeight + treasuryTimelockBlocks), and the
    // treasury balance recorded at the moment execution succeeded.
    std::uint64_t treasuryExecutableAtHeight = 0;
    utils::Amount treasuryBalanceBeforeExecution;
  };
  std::vector<GovernanceParameterChange> m_appliedChanges;
  std::vector<GovernanceParameterChange> m_pendingChanges;
  std::map<std::string, ProposalState> m_proposals;

  GovernanceExecutionResult
  executeApprovedProposal(const std::string &proposalId,
                          ProposalState &proposal, std::uint64_t currentHeight,
                          std::int64_t now, core::AccountStateView *accounts);

  GovernanceTallySnapshot computeTally(const std::string &proposalId,
                                       const ProposalState &proposal) const;

  void finalizeProposal(const std::string &proposalId, ProposalState &proposal,
                        std::uint64_t currentHeight, std::int64_t now,
                        core::AccountStateView *accounts,
                        std::uint64_t treasuryTimelockBlocks);

  static GovernanceParameterTarget
  parseTarget(const core::GovernanceProposalPayload &payload);
  static bool validateValue(GovernanceParameterTarget target,
                            const std::string &value);
  static bool proposalCanExecuteAt(const ProposalState &proposal,
                                   std::uint64_t currentHeight);
};

} // namespace nodo::node

#endif
