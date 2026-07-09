#include "node/GovernanceExecutor.hpp"

#include "core/ProtocolLimits.hpp"
#include "crypto/hash.h"
#include "node/ProtectionTreasury.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

std::string
governanceParameterTargetToString(GovernanceParameterTarget target) {
  switch (target) {
  case GovernanceParameterTarget::EPOCH_DURATION_SECONDS:
    return "EPOCH_DURATION_SECONDS";
  case GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT:
    return "MINIMUM_VALIDATOR_COUNT";
  case GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR:
    return "QUORUM_THRESHOLD_NUMERATOR";
  case GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR:
    return "QUORUM_THRESHOLD_DENOMINATOR";
  case GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK:
    return "MAX_TRANSACTIONS_PER_BLOCK";
  case GovernanceParameterTarget::MINIMUM_FEE_RAW:
    return "MINIMUM_FEE_RAW";
  case GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS:
    return "TREASURY_ALLOCATION_BASIS_POINTS";
  case GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS:
    return "VALIDATOR_REWARD_BASIS_POINTS";
  case GovernanceParameterTarget::UNKNOWN:
    return "UNKNOWN";
  }
  return "UNKNOWN";
}

GovernanceParameterTarget
governanceParameterTargetFromString(const std::string &s) {
  if (s == "EPOCH_DURATION_SECONDS")
    return GovernanceParameterTarget::EPOCH_DURATION_SECONDS;
  if (s == "MINIMUM_VALIDATOR_COUNT")
    return GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT;
  if (s == "QUORUM_THRESHOLD_NUMERATOR")
    return GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR;
  if (s == "QUORUM_THRESHOLD_DENOMINATOR")
    return GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR;
  if (s == "MAX_TRANSACTIONS_PER_BLOCK")
    return GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK;
  if (s == "MINIMUM_FEE_RAW")
    return GovernanceParameterTarget::MINIMUM_FEE_RAW;
  if (s == "TREASURY_ALLOCATION_BASIS_POINTS")
    return GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS;
  if (s == "VALIDATOR_REWARD_BASIS_POINTS")
    return GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS;
  return GovernanceParameterTarget::UNKNOWN;
}

std::string
governanceExecutionStatusToString(GovernanceExecutionStatus status) {
  switch (status) {
  case GovernanceExecutionStatus::APPLIED:
    return "APPLIED";
  case GovernanceExecutionStatus::PENDING:
    return "PENDING";
  case GovernanceExecutionStatus::REJECTED_UNKNOWN_TARGET:
    return "REJECTED_UNKNOWN_TARGET";
  case GovernanceExecutionStatus::REJECTED_INVALID_VALUE:
    return "REJECTED_INVALID_VALUE";
  case GovernanceExecutionStatus::REJECTED_NOT_YET_EFFECTIVE:
    return "REJECTED_NOT_YET_EFFECTIVE";
  }
  return "UNKNOWN";
}

std::string governanceProposalStatusToString(GovernanceProposalStatus status) {
  switch (status) {
  case GovernanceProposalStatus::PENDING:
    return "PENDING";
  case GovernanceProposalStatus::ACTIVE:
    return "ACTIVE";
  case GovernanceProposalStatus::APPROVED:
    return "APPROVED";
  case GovernanceProposalStatus::REJECTED:
    return "REJECTED";
  case GovernanceProposalStatus::EXPIRED:
    return "EXPIRED";
  case GovernanceProposalStatus::QUEUED_FOR_EXECUTION:
    return "QUEUED_FOR_EXECUTION";
  case GovernanceProposalStatus::EXECUTED:
    return "EXECUTED";
  case GovernanceProposalStatus::FAILED_EXECUTION:
    return "FAILED_EXECUTION";
  }
  return "FAILED_EXECUTION";
}

GovernanceTallySnapshot::GovernanceTallySnapshot()
    : m_yesWeight(0), m_noWeight(0), m_abstainWeight(0),
      m_participatingWeight(0), m_totalEligibleWeight(0), m_quorumMet(false),
      m_approvalThresholdMet(false) {}

GovernanceTallySnapshot::GovernanceTallySnapshot(
    std::string proposalId, std::uint64_t yesWeight, std::uint64_t noWeight,
    std::uint64_t abstainWeight, std::uint64_t participatingWeight,
    std::uint64_t totalEligibleWeight, bool quorumMet,
    bool approvalThresholdMet)
    : m_proposalId(std::move(proposalId)), m_yesWeight(yesWeight),
      m_noWeight(noWeight), m_abstainWeight(abstainWeight),
      m_participatingWeight(participatingWeight),
      m_totalEligibleWeight(totalEligibleWeight), m_quorumMet(quorumMet),
      m_approvalThresholdMet(approvalThresholdMet) {}

const std::string &GovernanceTallySnapshot::proposalId() const {
  return m_proposalId;
}
std::uint64_t GovernanceTallySnapshot::yesWeight() const { return m_yesWeight; }
std::uint64_t GovernanceTallySnapshot::noWeight() const { return m_noWeight; }
std::uint64_t GovernanceTallySnapshot::abstainWeight() const {
  return m_abstainWeight;
}
std::uint64_t GovernanceTallySnapshot::participatingWeight() const {
  return m_participatingWeight;
}
std::uint64_t GovernanceTallySnapshot::totalEligibleWeight() const {
  return m_totalEligibleWeight;
}
bool GovernanceTallySnapshot::quorumMet() const { return m_quorumMet; }
bool GovernanceTallySnapshot::approvalThresholdMet() const {
  return m_approvalThresholdMet;
}
bool GovernanceTallySnapshot::approved() const {
  return m_quorumMet && m_approvalThresholdMet;
}

std::string GovernanceTallySnapshot::serialize() const {
  std::ostringstream oss;
  oss << "GovernanceTallySnapshot{"
      << "proposalId=" << m_proposalId << ";yesWeight=" << m_yesWeight
      << ";noWeight=" << m_noWeight << ";abstainWeight=" << m_abstainWeight
      << ";participatingWeight=" << m_participatingWeight
      << ";totalEligibleWeight=" << m_totalEligibleWeight
      << ";quorumMet=" << (m_quorumMet ? 1 : 0)
      << ";approvalThresholdMet=" << (m_approvalThresholdMet ? 1 : 0) << "}";
  return oss.str();
}

// ---------------------------------------------------------------------------
// GovernanceParameterChange
// ---------------------------------------------------------------------------

GovernanceParameterChange::GovernanceParameterChange()
    : m_proposalId(""), m_target(GovernanceParameterTarget::UNKNOWN),
      m_previousValue(""), m_newValue(""), m_effectiveAtHeight(0),
      m_appliedAt(0) {}

GovernanceParameterChange::GovernanceParameterChange(
    std::string proposalId, GovernanceParameterTarget target,
    std::string previousValue, std::string newValue,
    std::uint64_t effectiveAtHeight, std::int64_t appliedAt)
    : m_proposalId(std::move(proposalId)), m_target(target),
      m_previousValue(std::move(previousValue)),
      m_newValue(std::move(newValue)), m_effectiveAtHeight(effectiveAtHeight),
      m_appliedAt(appliedAt) {}

GovernanceParameterChange GovernanceParameterChange::pending(
    std::string proposalId, GovernanceParameterTarget target,
    std::string newValue, std::uint64_t effectiveAtHeight) {
  return GovernanceParameterChange(std::move(proposalId), target,
                                   "", // previousValue unknown until applied
                                   std::move(newValue), effectiveAtHeight,
                                   0 // 0 = not yet applied
  );
}

const std::string &GovernanceParameterChange::proposalId() const {
  return m_proposalId;
}
GovernanceParameterTarget GovernanceParameterChange::target() const {
  return m_target;
}
const std::string &GovernanceParameterChange::previousValue() const {
  return m_previousValue;
}
const std::string &GovernanceParameterChange::newValue() const {
  return m_newValue;
}
std::uint64_t GovernanceParameterChange::effectiveAtHeight() const {
  return m_effectiveAtHeight;
}
std::int64_t GovernanceParameterChange::appliedAt() const {
  return m_appliedAt;
}

bool GovernanceParameterChange::isApplied() const { return m_appliedAt > 0; }

bool GovernanceParameterChange::isValid() const {
  return !m_proposalId.empty() &&
         m_target != GovernanceParameterTarget::UNKNOWN &&
         !m_newValue.empty() && m_effectiveAtHeight > 0;
}

std::string GovernanceParameterChange::serialize() const {
  std::ostringstream oss;
  oss << "GovernanceParameterChange{"
      << "proposalId=" << m_proposalId
      << ";target=" << governanceParameterTargetToString(m_target)
      << ";previousValue=" << m_previousValue << ";newValue=" << m_newValue
      << ";effectiveAtHeight=" << m_effectiveAtHeight
      << ";appliedAt=" << m_appliedAt << "}";
  return oss.str();
}

GovernanceParameterChange
GovernanceParameterChange::deserialize(const std::string &s) {
  // Helper: extract value for key= in key=value; format
  auto extractField = [&](const std::string &key) -> std::string {
    const std::string search = key + "=";
    const std::size_t pos = s.find(search);
    if (pos == std::string::npos) {
      return "";
    }
    const std::size_t valueStart = pos + search.size();
    const std::size_t valueEnd = s.find(';', valueStart);
    if (valueEnd == std::string::npos) {
      std::size_t end = s.find('}', valueStart);
      if (end == std::string::npos) {
        end = s.size();
      }
      return s.substr(valueStart, end - valueStart);
    }
    return s.substr(valueStart, valueEnd - valueStart);
  };

  const std::string proposalId = extractField("proposalId");
  const std::string targetStr = extractField("target");
  const std::string previousValue = extractField("previousValue");
  const std::string newValue = extractField("newValue");
  const std::string effectiveAtStr = extractField("effectiveAtHeight");
  const std::string appliedAtStr = extractField("appliedAt");

  std::uint64_t effectiveAt = 0;
  std::int64_t appliedAt = 0;
  try {
    effectiveAt = effectiveAtStr.empty() ? 0 : std::stoull(effectiveAtStr);
    appliedAt = appliedAtStr.empty() ? 0 : std::stoll(appliedAtStr);
  } catch (...) {
    throw std::invalid_argument(
        "Malformed numeric fields in GovernanceParameterChange");
  }

  return GovernanceParameterChange(
      proposalId, governanceParameterTargetFromString(targetStr), previousValue,
      newValue, effectiveAt, appliedAt);
}

// ---------------------------------------------------------------------------
// GovernanceExecutionResult
// ---------------------------------------------------------------------------

GovernanceExecutionResult::GovernanceExecutionResult(
    GovernanceExecutionStatus status, std::string detail,
    GovernanceParameterChange change)
    : m_status(status), m_detail(std::move(detail)),
      m_change(std::move(change)) {}

GovernanceExecutionResult
GovernanceExecutionResult::applied(GovernanceParameterChange change) {
  return GovernanceExecutionResult(
      GovernanceExecutionStatus::APPLIED,
      "Applied: " + governanceParameterTargetToString(change.target()) + " = " +
          change.newValue(),
      std::move(change));
}

GovernanceExecutionResult
GovernanceExecutionResult::applied(std::string detail) {
  return GovernanceExecutionResult(GovernanceExecutionStatus::APPLIED,
                                   std::move(detail),
                                   GovernanceParameterChange{});
}

GovernanceExecutionResult
GovernanceExecutionResult::pending(std::string proposalId,
                                   std::uint64_t effectiveAtHeight,
                                   std::uint64_t currentHeight) {
  std::ostringstream detail;
  detail << "Proposal " << proposalId << " effective at height "
         << effectiveAtHeight << " (current: " << currentHeight << ")";

  return GovernanceExecutionResult(GovernanceExecutionStatus::PENDING,
                                   detail.str(), GovernanceParameterChange{});
}

GovernanceExecutionResult
GovernanceExecutionResult::rejected(GovernanceExecutionStatus reason,
                                    std::string detail) {
  return GovernanceExecutionResult(reason, std::move(detail),
                                   GovernanceParameterChange{});
}

GovernanceExecutionStatus GovernanceExecutionResult::status() const {
  return m_status;
}
const std::string &GovernanceExecutionResult::detail() const {
  return m_detail;
}
bool GovernanceExecutionResult::isApplied() const {
  return m_status == GovernanceExecutionStatus::APPLIED;
}
bool GovernanceExecutionResult::isPending() const {
  return m_status == GovernanceExecutionStatus::PENDING;
}
const GovernanceParameterChange &GovernanceExecutionResult::change() const {
  return m_change;
}

std::string GovernanceExecutionResult::serialize() const {
  std::ostringstream oss;
  oss << "GovernanceExecutionResult{"
      << "status=" << governanceExecutionStatusToString(m_status)
      << ";detail=" << m_detail << "}";
  return oss.str();
}

// ---------------------------------------------------------------------------
// GovernanceExecutor — private helpers
// ---------------------------------------------------------------------------

namespace {

bool parseUint64Strict(const std::string &value, std::uint64_t &parsedValue) {
  if (value.empty()) {
    return false;
  }
  for (const char character : value) {
    if (character < '0' || character > '9') {
      return false;
    }
  }

  try {
    std::size_t parsedCharacters = 0;
    const unsigned long long parsed = std::stoull(value, &parsedCharacters);
    if (parsedCharacters != value.size() ||
        parsed > std::numeric_limits<std::uint64_t>::max() ||
        std::to_string(parsed) != value) {
      return false;
    }
    parsedValue = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool checkedAddU64(std::uint64_t left, std::uint64_t right,
                   std::uint64_t &out) {
  if (std::numeric_limits<std::uint64_t>::max() - left < right) {
    return false;
  }
  out = left + right;
  return true;
}

bool ratioMet(std::uint64_t value, std::uint64_t denominator,
              std::uint64_t total, std::uint64_t numerator) {
  return static_cast<unsigned __int128>(value) *
             static_cast<unsigned __int128>(denominator) >=
         static_cast<unsigned __int128>(total) *
             static_cast<unsigned __int128>(numerator);
}

std::string hashString(const std::string &value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_bytes(reinterpret_cast<const unsigned char *>(value.data()),
                  static_cast<unsigned long long>(value.size()), output,
                  sizeof(output));
  return std::string(output);
}

} // namespace

GovernanceParameterTarget GovernanceExecutor::parseTarget(
    const core::GovernanceProposalPayload &payload) {
  return governanceParameterTargetFromString(payload.parameterTarget());
}

bool GovernanceExecutor::validateValue(GovernanceParameterTarget target,
                                       const std::string &value) {
  if (value.empty()) {
    return false;
  }

  std::uint64_t numericValue = 0;
  if (!parseUint64Strict(value, numericValue)) {
    return false;
  }

  switch (target) {
  case GovernanceParameterTarget::EPOCH_DURATION_SECONDS:
    return numericValue >= 60; // at least 1 minute

  case GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT:
    return numericValue >= 1;

  case GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR:
    return numericValue >= 1;

  case GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR:
    return numericValue >= 2;

  case GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK:
    return numericValue >= 1 &&
           numericValue <= core::ProtocolLimits::MAX_BLOCK_RECORDS;

  case GovernanceParameterTarget::MINIMUM_FEE_RAW:
    return numericValue <=
           static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

  case GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS:
    return numericValue <= 10000; // max 100%

  case GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS:
    return numericValue <= 10000; // max 100%

  case GovernanceParameterTarget::UNKNOWN:
    return false;
  }

  return false;
}

bool GovernanceExecutor::proposalCanExecuteAt(const ProposalState &proposal,
                                              std::uint64_t currentHeight) {
  if (proposal.status != GovernanceProposalStatus::APPROVED &&
      proposal.status != GovernanceProposalStatus::QUEUED_FOR_EXECUTION) {
    return false;
  }
  // Treasury spends never auto-execute from a height advance: they require
  // an explicit GOVERNANCE_EXECUTE transaction once eligible (see
  // proposalReadyForExplicitExecution), so anyone can observe an approved
  // spend before it moves funds.
  if (proposal.payload.type() == core::GovernanceProposalType::TREASURY_SPEND) {
    return false;
  }
  if (proposal.payload.type() !=
      core::GovernanceProposalType::PARAMETER_CHANGE) {
    return true;
  }
  return currentHeight >= proposal.payload.parameterEffectiveHeight();
}

bool GovernanceExecutor::proposalReadyForExplicitExecution(
    const std::string &proposalId, std::uint64_t currentHeight) const {
  const auto found = m_proposals.find(proposalId);
  if (found == m_proposals.end()) {
    return false;
  }
  const ProposalState &proposal = found->second;
  return proposal.status == GovernanceProposalStatus::QUEUED_FOR_EXECUTION &&
         proposal.payload.type() ==
             core::GovernanceProposalType::TREASURY_SPEND &&
         currentHeight >= proposal.treasuryExecutableAtHeight;
}

utils::Amount GovernanceExecutor::treasuryEpochSpentSoFar(
    std::uint64_t currentHeight, const std::string &excludingProposalId) const {
  const std::uint64_t currentEpoch =
      ValidatorLifecycle::epochIndexForBlock(currentHeight);
  utils::Amount total;
  for (const auto &[id, proposal] : m_proposals) {
    if (id == excludingProposalId) {
      continue;
    }
    if (proposal.payload.type() !=
            core::GovernanceProposalType::TREASURY_SPEND ||
        proposal.status != GovernanceProposalStatus::EXECUTED) {
      continue;
    }
    if (ValidatorLifecycle::epochIndexForBlock(proposal.executedAtHeight) !=
        currentEpoch) {
      continue;
    }
    total = total +
            utils::Amount::fromRawUnits(proposal.payload.treasuryAmountRaw());
  }
  return total;
}

GovernanceTallySnapshot
GovernanceExecutor::computeTally(const std::string &proposalId,
                                 const ProposalState &proposal) const {
  std::uint64_t yes = 0;
  std::uint64_t no = 0;
  std::uint64_t abstain = 0;
  for (const auto &[_, vote] : proposal.votes) {
    std::uint64_t *target = &no;
    if (vote.choice == core::GovernanceVoteChoice::YES) {
      target = &yes;
    } else if (vote.choice == core::GovernanceVoteChoice::ABSTAIN) {
      target = &abstain;
    }
    if (!checkedAddU64(*target, vote.weight, *target)) {
      throw std::overflow_error("Governance tally overflow.");
    }
  }

  std::uint64_t participating = 0;
  if (!checkedAddU64(yes, no, participating) ||
      !checkedAddU64(participating, abstain, participating)) {
    throw std::overflow_error("Governance participant tally overflow.");
  }

  const bool quorumMet =
      proposal.totalEligibleWeight > 0 &&
      ratioMet(participating, proposal.payload.quorumDenominator(),
               proposal.totalEligibleWeight,
               proposal.payload.quorumNumerator());

  std::uint64_t directional = 0;
  if (!checkedAddU64(yes, no, directional)) {
    throw std::overflow_error("Governance directional tally overflow.");
  }

  const bool approvalMet =
      directional > 0 &&
      ratioMet(yes, proposal.payload.approvalDenominator(), directional,
               proposal.payload.approvalNumerator());

  return GovernanceTallySnapshot(proposalId, yes, no, abstain, participating,
                                 proposal.totalEligibleWeight, quorumMet,
                                 approvalMet);
}

GovernanceExecutionResult GovernanceExecutor::executeApprovedProposal(
    const std::string &proposalId, ProposalState &proposal,
    std::uint64_t currentHeight, std::int64_t now,
    [[maybe_unused]] core::AccountStateView *accounts) {
  if (proposal.status == GovernanceProposalStatus::EXECUTED) {
    return GovernanceExecutionResult::rejected(
        GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
        "Proposal " + proposalId + " has already been executed.");
  }

  if (proposal.payload.type() == core::GovernanceProposalType::TEXT) {
    proposal.status = GovernanceProposalStatus::EXECUTED;
    proposal.executedAtHeight = currentHeight;
    proposal.executedAt = now;
    proposal.executionDetail = "Text proposal recorded on-chain.";
    return GovernanceExecutionResult::applied(proposal.executionDetail);
  }

  // Treasury spends never reach here: finalizeProposal queues them for
  // execution instead of calling executeApprovedProposal (see
  // executeQueuedTreasuryProposal), since they require an explicit,
  // evidence-backed GOVERNANCE_EXECUTE transaction rather than immediate
  // automatic execution.

  const GovernanceParameterTarget target = parseTarget(proposal.payload);
  if (target == GovernanceParameterTarget::UNKNOWN) {
    return GovernanceExecutionResult::rejected(
        GovernanceExecutionStatus::REJECTED_UNKNOWN_TARGET,
        "Unknown governance parameter target in proposal " + proposalId);
  }

  if (!validateValue(target, proposal.payload.parameterValue())) {
    return GovernanceExecutionResult::rejected(
        GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
        "Invalid value '" + proposal.payload.parameterValue() +
            "' for target " + governanceParameterTargetToString(target));
  }

  if (currentHeight < proposal.payload.parameterEffectiveHeight()) {
    bool alreadyPending = false;
    for (const auto &pc : m_pendingChanges) {
      if (pc.proposalId() == proposalId) {
        alreadyPending = true;
        break;
      }
    }
    if (!alreadyPending) {
      m_pendingChanges.push_back(GovernanceParameterChange::pending(
          proposalId, target, proposal.payload.parameterValue(),
          proposal.payload.parameterEffectiveHeight()));
    }
    proposal.status = GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
    proposal.executionDetail =
        "Parameter change queued for height " +
        std::to_string(proposal.payload.parameterEffectiveHeight());
    return GovernanceExecutionResult::pending(
        proposalId, proposal.payload.parameterEffectiveHeight(), currentHeight);
  }

  const std::string previousValue = currentValueForTarget(target);
  GovernanceParameterChange appliedChange(
      proposalId, target, previousValue, proposal.payload.parameterValue(),
      proposal.payload.parameterEffectiveHeight(), now);
  m_appliedChanges.push_back(appliedChange);
  m_pendingChanges.erase(
      std::remove_if(m_pendingChanges.begin(), m_pendingChanges.end(),
                     [&proposalId](const GovernanceParameterChange &pc) {
                       return pc.proposalId() == proposalId;
                     }),
      m_pendingChanges.end());
  proposal.status = GovernanceProposalStatus::EXECUTED;
  proposal.executedAtHeight = currentHeight;
  proposal.executedAt = now;
  proposal.executionDetail = appliedChange.serialize();
  return GovernanceExecutionResult::applied(appliedChange);
}

void GovernanceExecutor::finalizeProposal(
    const std::string &proposalId, ProposalState &proposal,
    std::uint64_t currentHeight, std::int64_t now,
    core::AccountStateView *accounts, std::uint64_t treasuryTimelockBlocks) {
  proposal.finalTally = computeTally(proposalId, proposal);
  proposal.decidedAtHeight = currentHeight;
  proposal.decidedAt = now;

  if (!proposal.finalTally.quorumMet()) {
    proposal.status = GovernanceProposalStatus::EXPIRED;
    proposal.executionDetail = "Voting period ended without quorum.";
    return;
  }

  if (!proposal.finalTally.approvalThresholdMet()) {
    proposal.status = GovernanceProposalStatus::REJECTED;
    proposal.executionDetail =
        "Voting period ended without approval threshold.";
    return;
  }

  proposal.status = GovernanceProposalStatus::APPROVED;

  if (proposal.payload.type() == core::GovernanceProposalType::TREASURY_SPEND) {
    // Decision and execution are deliberately separate steps for treasury
    // spends: approval only starts the timelock. Moving funds requires a
    // later, explicit GOVERNANCE_EXECUTE transaction (see
    // executeQueuedTreasuryProposal) so the spend is never silently
    // triggered by a block advance alone.
    proposal.treasuryExecutableAtHeight =
        currentHeight + treasuryTimelockBlocks;
    proposal.status = GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
    proposal.executionDetail =
        "Treasury spend approved; executable via governance execute at "
        "height " +
        std::to_string(proposal.treasuryExecutableAtHeight) + " or later.";
    return;
  }

  const GovernanceExecutionResult execution = executeApprovedProposal(
      proposalId, proposal, currentHeight, now, accounts);
  if (execution.isPending()) {
    proposal.status = GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
    return;
  }
  if (!execution.isApplied()) {
    proposal.status = GovernanceProposalStatus::FAILED_EXECUTION;
    proposal.executionDetail = execution.detail();
  }
}

// ---------------------------------------------------------------------------
// GovernanceExecutor — public interface
// ---------------------------------------------------------------------------

GovernanceExecutor::GovernanceExecutor() = default;

bool GovernanceExecutor::validateProposalPayload(
    const std::string &proposalPayload, std::string &reason) {
  try {
    const core::GovernanceProposalPayload payload =
        core::GovernanceProposalPayload::deserialize(proposalPayload);
    if (payload.type() == core::GovernanceProposalType::PARAMETER_CHANGE) {
      const GovernanceParameterTarget target = parseTarget(payload);
      if (target == GovernanceParameterTarget::UNKNOWN ||
          !validateValue(target, payload.parameterValue())) {
        reason = "Governance parameter change payload is invalid.";
        return false;
      }
    }
  } catch (const std::exception &error) {
    reason = error.what();
    return false;
  }
  reason.clear();
  return true;
}

bool GovernanceExecutor::submitProposal(
    const std::string &proposalId, const std::string &proposerAddress,
    const std::string &proposalPayload, std::uint64_t currentHeight,
    std::int64_t now, std::uint64_t totalEligibleWeight, std::string &reason) {
  if (proposalId.empty() || proposerAddress.empty() || currentHeight == 0 ||
      now <= 0 || totalEligibleWeight == 0) {
    reason = "Governance proposal identity, block context, or voting snapshot "
             "is invalid.";
    return false;
  }
  if (hasProposal(proposalId)) {
    reason = "Governance proposal already exists.";
    return false;
  }
  if (!validateProposalPayload(proposalPayload, reason)) {
    return false;
  }

  const core::GovernanceProposalPayload payload =
      core::GovernanceProposalPayload::deserialize(proposalPayload);
  std::uint64_t votingStart = 0;
  if (!checkedAddU64(currentHeight, payload.votingStartDelayBlocks(),
                     votingStart)) {
    reason = "Governance voting start height overflow.";
    return false;
  }
  std::uint64_t votingEnd = 0;
  if (!checkedAddU64(votingStart, payload.votingPeriodBlocks() - 1,
                     votingEnd)) {
    reason = "Governance voting end height overflow.";
    return false;
  }

  ProposalState state;
  state.proposerAddress = proposerAddress;
  state.payload = payload;
  state.createdHeight = currentHeight;
  state.createdAt = now;
  state.votingStartHeight = votingStart;
  state.votingEndHeight = votingEnd;
  state.totalEligibleWeight = totalEligibleWeight;
  state.status = currentHeight >= votingStart
                     ? GovernanceProposalStatus::ACTIVE
                     : GovernanceProposalStatus::PENDING;
  m_proposals.emplace(proposalId, std::move(state));
  reason.clear();
  return true;
}

bool GovernanceExecutor::castVote(const std::string &proposalId,
                                  const std::string &validatorAddress,
                                  core::GovernanceVoteChoice choice,
                                  std::uint64_t votingWeight,
                                  std::uint64_t currentHeight, std::int64_t now,
                                  const std::string &transactionId,
                                  std::string &reason) {
  auto found = m_proposals.find(proposalId);
  if (found == m_proposals.end()) {
    reason = "Governance proposal does not exist.";
    return false;
  }
  ProposalState &proposal = found->second;
  if (proposal.status == GovernanceProposalStatus::APPROVED ||
      proposal.status == GovernanceProposalStatus::REJECTED ||
      proposal.status == GovernanceProposalStatus::EXPIRED ||
      proposal.status == GovernanceProposalStatus::QUEUED_FOR_EXECUTION ||
      proposal.status == GovernanceProposalStatus::EXECUTED ||
      proposal.status == GovernanceProposalStatus::FAILED_EXECUTION) {
    reason = "Governance proposal is already decided.";
    return false;
  }
  if (validatorAddress.empty() || transactionId.empty() || votingWeight == 0 ||
      votingWeight > proposal.totalEligibleWeight ||
      currentHeight < proposal.createdHeight || now <= 0) {
    reason = "Governance vote context or weight is invalid.";
    return false;
  }
  if (currentHeight < proposal.votingStartHeight) {
    reason = "Governance voting period has not started.";
    return false;
  }
  if (currentHeight > proposal.votingEndHeight) {
    reason = "Governance voting period has ended.";
    return false;
  }
  if (proposal.votes.find(validatorAddress) != proposal.votes.end()) {
    reason = "Validator has already voted on this proposal.";
    return false;
  }

  proposal.status = GovernanceProposalStatus::ACTIVE;
  proposal.votes.emplace(
      validatorAddress,
      VoteState{choice, votingWeight, currentHeight, now, transactionId});
  reason.clear();
  return true;
}

bool GovernanceExecutor::hasProposal(const std::string &proposalId) const {
  return m_proposals.find(proposalId) != m_proposals.end();
}

bool GovernanceExecutor::hasVote(const std::string &proposalId,
                                 const std::string &validatorAddress) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal != m_proposals.end() &&
         proposal->second.votes.find(validatorAddress) !=
             proposal->second.votes.end();
}

bool GovernanceExecutor::proposalApproved(const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal != m_proposals.end() &&
         (proposal->second.status == GovernanceProposalStatus::APPROVED ||
          proposal->second.status ==
              GovernanceProposalStatus::QUEUED_FOR_EXECUTION ||
          proposal->second.status == GovernanceProposalStatus::EXECUTED);
}

bool GovernanceExecutor::proposalOpenForVoting(const std::string &proposalId,
                                               std::uint64_t height) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal != m_proposals.end() &&
         height >= proposal->second.votingStartHeight &&
         height <= proposal->second.votingEndHeight &&
         (proposal->second.status == GovernanceProposalStatus::PENDING ||
          proposal->second.status == GovernanceProposalStatus::ACTIVE);
}

std::uint64_t GovernanceExecutor::proposalVotingStartHeight(
    const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal == m_proposals.end() ? 0 : proposal->second.votingStartHeight;
}

std::uint64_t GovernanceExecutor::proposalVotingEndHeight(
    const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal == m_proposals.end() ? 0 : proposal->second.votingEndHeight;
}

GovernanceProposalStatus
GovernanceExecutor::proposalStatus(const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal == m_proposals.end()
             ? GovernanceProposalStatus::FAILED_EXECUTION
             : proposal->second.status;
}

GovernanceTallySnapshot
GovernanceExecutor::tallyForProposal(const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  if (proposal == m_proposals.end()) {
    return GovernanceTallySnapshot();
  }
  if (proposal->second.decidedAtHeight > 0) {
    return proposal->second.finalTally;
  }
  return computeTally(proposalId, proposal->second);
}

std::string
GovernanceExecutor::proposalDetail(const std::string &proposalId) const {
  const auto proposalIt = m_proposals.find(proposalId);
  if (proposalIt == m_proposals.end()) {
    return "";
  }

  const ProposalState &proposal = proposalIt->second;
  const core::GovernanceProposalPayload &payload = proposal.payload;
  const GovernanceTallySnapshot tally = computeTally(proposalId, proposal);

  std::ostringstream oss;
  oss << "GovernanceProposal{"
      << "id=" << proposalId << ";proposer=" << proposal.proposerAddress
      << ";type=" << core::governanceProposalTypeToString(payload.type())
      << ";title=" << payload.title()
      << ";description=" << payload.description()
      << ";votingStartDelayBlocks=" << payload.votingStartDelayBlocks()
      << ";votingPeriodBlocks=" << payload.votingPeriodBlocks()
      << ";quorum=" << payload.quorumNumerator() << "/"
      << payload.quorumDenominator()
      << ";approval=" << payload.approvalNumerator() << "/"
      << payload.approvalDenominator()
      << ";parameterTarget=" << payload.parameterTarget()
      << ";parameterValue=" << payload.parameterValue()
      << ";parameterEffectiveHeight=" << payload.parameterEffectiveHeight()
      << ";treasuryRecipient=" << payload.treasuryRecipient()
      << ";treasuryAmountRaw=" << payload.treasuryAmountRaw()
      << ";createdHeight=" << proposal.createdHeight
      << ";createdAt=" << proposal.createdAt
      << ";votingStartHeight=" << proposal.votingStartHeight
      << ";votingEndHeight=" << proposal.votingEndHeight
      << ";totalEligibleWeight=" << proposal.totalEligibleWeight
      << ";status=" << governanceProposalStatusToString(proposal.status)
      << ";decidedAtHeight=" << proposal.decidedAtHeight
      << ";executedAtHeight=" << proposal.executedAtHeight
      << ";tally=" << tally.serialize()
      << ";executionDetail=" << proposal.executionDetail << "}";
  return oss.str();
}

std::string
GovernanceExecutor::proposalVotes(const std::string &proposalId) const {
  const auto proposalIt = m_proposals.find(proposalId);
  if (proposalIt == m_proposals.end()) {
    return "";
  }

  std::ostringstream oss;
  oss << "GovernanceVotes{proposalId=" << proposalId
      << ";count=" << proposalIt->second.votes.size();
  for (const auto &[validator, vote] : proposalIt->second.votes) {
    oss << ";vote={validator=" << validator
        << ";choice=" << core::governanceVoteChoiceToString(vote.choice)
        << ";weight=" << vote.weight << ";castHeight=" << vote.castHeight
        << ";castAt=" << vote.castAt << ";transactionId=" << vote.transactionId
        << "}";
  }
  oss << "}";
  return oss.str();
}

std::string GovernanceExecutor::proposalExecutionDetail(
    const std::string &proposalId) const {
  const auto proposalIt = m_proposals.find(proposalId);
  if (proposalIt == m_proposals.end()) {
    return "";
  }

  std::ostringstream oss;
  oss << "GovernanceExecution{"
      << "proposalId=" << proposalId << ";status="
      << governanceProposalStatusToString(proposalIt->second.status)
      << ";executed="
      << (proposalIt->second.status == GovernanceProposalStatus::EXECUTED ? 1
                                                                          : 0)
      << ";executedAtHeight=" << proposalIt->second.executedAtHeight
      << ";executedAt=" << proposalIt->second.executedAt
      << ";detail=" << proposalIt->second.executionDetail << "}";
  return oss.str();
}

std::size_t GovernanceExecutor::advanceToHeight(
    std::uint64_t currentHeight, std::int64_t now,
    core::AccountStateView *accounts, std::uint64_t treasuryTimelockBlocks) {
  if (currentHeight == 0 || now <= 0) {
    throw std::invalid_argument("Governance activation boundary is invalid.");
  }

  std::size_t executed = 0;
  for (auto &[proposalId, proposal] : m_proposals) {
    const GovernanceProposalStatus before = proposal.status;
    if (proposal.status == GovernanceProposalStatus::PENDING &&
        currentHeight >= proposal.votingStartHeight) {
      proposal.status = GovernanceProposalStatus::ACTIVE;
    }
    if (proposal.status == GovernanceProposalStatus::ACTIVE &&
        currentHeight > proposal.votingEndHeight) {
      finalizeProposal(proposalId, proposal, currentHeight, now, accounts,
                       treasuryTimelockBlocks);
    } else if (proposal.status ==
                   GovernanceProposalStatus::QUEUED_FOR_EXECUTION &&
               proposalCanExecuteAt(proposal, currentHeight)) {
      const GovernanceExecutionResult execution = executeApprovedProposal(
          proposalId, proposal, currentHeight, now, accounts);
      if (!execution.isApplied() && !execution.isPending()) {
        proposal.status = GovernanceProposalStatus::FAILED_EXECUTION;
        proposal.executionDetail = execution.detail();
      }
    }
    if (before != GovernanceProposalStatus::EXECUTED &&
        proposal.status == GovernanceProposalStatus::EXECUTED) {
      ++executed;
    }
  }

  return executed;
}

GovernanceExecutor::TreasuryExecutionOutcome
GovernanceExecutor::executeQueuedTreasuryProposal(
    const std::string &proposalId, std::uint64_t currentHeight,
    std::int64_t now, const economics::TreasuryPolicy &treasuryPolicy,
    const economics::TreasuryApproval &approval,
    core::AccountStateView &accounts) {
  TreasuryExecutionOutcome outcome;

  if (!proposalReadyForExplicitExecution(proposalId, currentHeight)) {
    outcome.reason = "Treasury proposal " + proposalId +
                     " is not eligible for execution at height " +
                     std::to_string(currentHeight) + ".";
    return outcome;
  }

  ProposalState &proposal = m_proposals.at(proposalId);
  const utils::Amount amount =
      utils::Amount::fromRawUnits(proposal.payload.treasuryAmountRaw());
  const core::AccountState treasuryAccount =
      accounts.accountOrDefault(ProtectionTreasury::TREASURY_ADDRESS);
  const std::uint64_t currentEpoch =
      ValidatorLifecycle::epochIndexForBlock(currentHeight);

  const economics::TreasuryProposal treasuryProposal(
      proposalId, proposal.payload.treasuryRecipient(), amount,
      proposal.payload.title(), proposal.decidedAtHeight, currentEpoch,
      proposal.proposerAddress);

  const economics::TreasuryAccount treasury(
      "treasury-main", ProtectionTreasury::TREASURY_ADDRESS,
      treasuryAccount.balance(), currentEpoch, false, "unlocked");

  const economics::TreasurySpendValidationResult spendResult =
      economics::TreasurySpendValidator::validateSpend(
          economics::DefenseModeState::INACTIVE,
          economics::DefenseModePolicy::defaultPolicy(), treasury,
          treasuryPolicy, treasuryProposal, approval, currentHeight,
          treasuryEpochSpentSoFar(currentHeight, proposalId));

  if (!spendResult.accepted()) {
    outcome.reason =
        economics::treasurySpendStatusToString(spendResult.status()) + ": " +
        spendResult.reason();
    return outcome;
  }

  const core::AccountState recipient =
      accounts.accountOrDefault(proposal.payload.treasuryRecipient());
  if (!accounts.putAccount(core::AccountState(
          ProtectionTreasury::TREASURY_ADDRESS,
          treasuryAccount.balance() - amount, treasuryAccount.nonce())) ||
      !accounts.putAccount(core::AccountState(
          proposal.payload.treasuryRecipient(), recipient.balance() + amount,
          recipient.nonce()))) {
    outcome.reason = "Treasury spend account write failed.";
    return outcome;
  }

  proposal.status = GovernanceProposalStatus::EXECUTED;
  proposal.executedAtHeight = currentHeight;
  proposal.executedAt = now;
  proposal.treasuryBalanceBeforeExecution = treasuryAccount.balance();
  proposal.executionDetail =
      "Treasury spend executed to " + proposal.payload.treasuryRecipient() +
      " amountRaw=" + std::to_string(proposal.payload.treasuryAmountRaw());

  outcome.applied = true;
  outcome.spendRecord = spendResult.spendRecord();
  outcome.treasuryBalanceBefore = treasuryAccount.balance();
  return outcome;
}

GovernanceExecutor::GovernanceProposalSnapshot
GovernanceExecutor::proposalSnapshot(const std::string &proposalId) const {
  GovernanceProposalSnapshot snapshot;
  const auto found = m_proposals.find(proposalId);
  if (found == m_proposals.end()) {
    return snapshot;
  }

  const ProposalState &proposal = found->second;
  snapshot.proposalId = proposalId;
  snapshot.proposerAddress = proposal.proposerAddress;
  snapshot.payload = proposal.payload;
  snapshot.createdHeight = proposal.createdHeight;
  snapshot.createdAt = proposal.createdAt;
  snapshot.votingStartHeight = proposal.votingStartHeight;
  snapshot.votingEndHeight = proposal.votingEndHeight;
  snapshot.totalEligibleWeight = proposal.totalEligibleWeight;
  snapshot.status = proposal.status;
  snapshot.tally = proposal.decidedAtHeight > 0
                       ? proposal.finalTally
                       : computeTally(proposalId, proposal);
  snapshot.decidedAtHeight = proposal.decidedAtHeight;
  snapshot.decidedAt = proposal.decidedAt;
  snapshot.executedAtHeight = proposal.executedAtHeight;
  snapshot.executedAt = proposal.executedAt;
  snapshot.treasuryBalanceBeforeExecution =
      proposal.treasuryBalanceBeforeExecution;
  for (const auto &[validator, vote] : proposal.votes) {
    snapshot.votes.push_back(
        GovernanceVoteInfo{validator, vote.choice, vote.weight, vote.castHeight,
                           vote.castAt, vote.transactionId});
  }
  return snapshot;
}

std::vector<GovernanceExecutor::ProposalRecord>
GovernanceExecutor::allProposalRecords() const {
  std::vector<ProposalRecord> records;
  records.reserve(m_proposals.size());
  for (const auto &[proposalId, proposal] : m_proposals) {
    ProposalRecord record;
    record.proposalId = proposalId;
    record.proposerAddress = proposal.proposerAddress;
    record.payload = proposal.payload;
    record.createdHeight = proposal.createdHeight;
    record.createdAt = proposal.createdAt;
    record.votingStartHeight = proposal.votingStartHeight;
    record.votingEndHeight = proposal.votingEndHeight;
    record.totalEligibleWeight = proposal.totalEligibleWeight;
    record.status = proposal.status;
    record.finalTally = proposal.finalTally;
    record.decidedAtHeight = proposal.decidedAtHeight;
    record.decidedAt = proposal.decidedAt;
    record.executedAtHeight = proposal.executedAtHeight;
    record.executedAt = proposal.executedAt;
    record.executionDetail = proposal.executionDetail;
    record.treasuryExecutableAtHeight = proposal.treasuryExecutableAtHeight;
    record.treasuryBalanceBeforeExecution =
        proposal.treasuryBalanceBeforeExecution;
    for (const auto &[validator, vote] : proposal.votes) {
      record.votes.push_back(
          GovernanceVoteInfo{validator, vote.choice, vote.weight,
                             vote.castHeight, vote.castAt, vote.transactionId});
    }
    records.push_back(std::move(record));
  }
  return records;
}

GovernanceExecutor GovernanceExecutor::restore(
    std::vector<GovernanceParameterChange> appliedChanges,
    std::vector<GovernanceParameterChange> pendingChanges,
    std::vector<ProposalRecord> proposals) {
  GovernanceExecutor executor;
  executor.m_appliedChanges = std::move(appliedChanges);
  executor.m_pendingChanges = std::move(pendingChanges);

  for (const ProposalRecord &record : proposals) {
    if (record.proposalId.empty()) {
      throw std::invalid_argument(
          "Restored governance proposal id is empty.");
    }

    ProposalState state;
    state.proposerAddress = record.proposerAddress;
    state.payload = record.payload;
    state.createdHeight = record.createdHeight;
    state.createdAt = record.createdAt;
    state.votingStartHeight = record.votingStartHeight;
    state.votingEndHeight = record.votingEndHeight;
    state.totalEligibleWeight = record.totalEligibleWeight;
    state.status = record.status;
    state.finalTally = record.finalTally;
    state.decidedAtHeight = record.decidedAtHeight;
    state.decidedAt = record.decidedAt;
    state.executedAtHeight = record.executedAtHeight;
    state.executedAt = record.executedAt;
    state.executionDetail = record.executionDetail;
    state.treasuryExecutableAtHeight = record.treasuryExecutableAtHeight;
    state.treasuryBalanceBeforeExecution =
        record.treasuryBalanceBeforeExecution;

    for (const GovernanceVoteInfo &vote : record.votes) {
      VoteState voteState;
      voteState.choice = vote.choice;
      voteState.weight = vote.weight;
      voteState.castHeight = vote.castHeight;
      voteState.castAt = vote.castAt;
      voteState.transactionId = vote.transactionId;
      state.votes.emplace(vote.validatorAddress, std::move(voteState));
    }

    if (!executor.m_proposals.emplace(record.proposalId, std::move(state))
             .second) {
      throw std::invalid_argument(
          "Duplicate governance proposal id during restore: " +
          record.proposalId);
    }
  }

  return executor;
}

const std::vector<GovernanceParameterChange> &
GovernanceExecutor::appliedChanges() const {
  return m_appliedChanges;
}

std::vector<GovernanceParameterChange>
GovernanceExecutor::pendingChanges() const {
  return m_pendingChanges;
}

bool GovernanceExecutor::hasBeenExecuted(const std::string &proposalId) const {
  const auto proposal = m_proposals.find(proposalId);
  return proposal != m_proposals.end() &&
         proposal->second.status == GovernanceProposalStatus::EXECUTED;
}

std::string GovernanceExecutor::currentValueForTarget(
    GovernanceParameterTarget target) const {
  // Scan applied changes in reverse to get the most recent value
  for (auto it = m_appliedChanges.rbegin(); it != m_appliedChanges.rend();
       ++it) {
    if (it->target() == target) {
      return it->newValue();
    }
  }
  return "";
}

std::size_t GovernanceExecutor::activeProposalCount() const {
  std::size_t count = 0;
  for (const auto &[_, proposal] : m_proposals) {
    if (proposal.status == GovernanceProposalStatus::PENDING ||
        proposal.status == GovernanceProposalStatus::ACTIVE) {
      ++count;
    }
  }
  return count;
}

std::size_t GovernanceExecutor::approvedProposalCount() const {
  std::size_t count = 0;
  for (const auto &[_, proposal] : m_proposals) {
    if (proposal.status == GovernanceProposalStatus::APPROVED ||
        proposal.status == GovernanceProposalStatus::QUEUED_FOR_EXECUTION ||
        proposal.status == GovernanceProposalStatus::EXECUTED) {
      ++count;
    }
  }
  return count;
}

std::size_t
GovernanceExecutor::executableProposalCount(std::uint64_t currentHeight) const {
  std::size_t count = 0;
  for (const auto &[_, proposal] : m_proposals) {
    if (proposalCanExecuteAt(proposal, currentHeight)) {
      ++count;
    }
  }
  return count;
}

std::size_t GovernanceExecutor::executedProposalCount() const {
  std::size_t count = 0;
  for (const auto &[_, proposal] : m_proposals) {
    if (proposal.status == GovernanceProposalStatus::EXECUTED) {
      ++count;
    }
  }
  return count;
}

std::vector<std::string> GovernanceExecutor::proposalIds() const {
  std::vector<std::string> ids;
  ids.reserve(m_proposals.size());
  for (const auto &[id, _] : m_proposals) {
    ids.push_back(id);
  }
  return ids;
}

std::string GovernanceExecutor::serialize() const {
  std::ostringstream oss;
  oss << "GovernanceExecutor{"
      << "appliedCount=" << m_appliedChanges.size();
  for (const auto &change : m_appliedChanges)
    oss << ";applied=" << change.serialize();
  oss << ";pendingCount=" << m_pendingChanges.size();
  for (const auto &change : m_pendingChanges)
    oss << ";pending=" << change.serialize();
  oss << ";proposalCount=" << m_proposals.size();
  for (const auto &[id, proposal] : m_proposals) {
    const std::string payloadDigest = hashString(proposal.payload.serialize());
    oss << ";proposal={id=" << id << ",proposer=" << proposal.proposerAddress
        << ",type="
        << core::governanceProposalTypeToString(proposal.payload.type())
        << ",title=" << proposal.payload.title()
        << ",description=" << proposal.payload.description()
        << ",payloadDigest=" << payloadDigest << ",votingStartDelayBlocks="
        << proposal.payload.votingStartDelayBlocks()
        << ",votingPeriodBlocks=" << proposal.payload.votingPeriodBlocks()
        << ",quorum=" << proposal.payload.quorumNumerator() << '/'
        << proposal.payload.quorumDenominator()
        << ",approval=" << proposal.payload.approvalNumerator() << '/'
        << proposal.payload.approvalDenominator()
        << ",parameterTarget=" << proposal.payload.parameterTarget()
        << ",parameterValue=" << proposal.payload.parameterValue()
        << ",parameterEffectiveHeight="
        << proposal.payload.parameterEffectiveHeight()
        << ",treasuryRecipient=" << proposal.payload.treasuryRecipient()
        << ",treasuryAmountRaw=" << proposal.payload.treasuryAmountRaw()
        << ",createdHeight=" << proposal.createdHeight
        << ",createdAt=" << proposal.createdAt
        << ",votingStartHeight=" << proposal.votingStartHeight
        << ",votingEndHeight=" << proposal.votingEndHeight
        << ",totalEligibleWeight=" << proposal.totalEligibleWeight
        << ",status=" << governanceProposalStatusToString(proposal.status)
        << ",decidedAtHeight=" << proposal.decidedAtHeight
        << ",executedAtHeight=" << proposal.executedAtHeight
        << ",finalTally=" << proposal.finalTally.serialize()
        << ",executionDetail=" << proposal.executionDetail
        << ",voteCount=" << proposal.votes.size();
    for (const auto &[validator, vote] : proposal.votes) {
      oss << ",vote=" << validator << ':'
          << core::governanceVoteChoiceToString(vote.choice) << ':'
          << vote.weight << ':' << vote.castHeight << ':' << vote.transactionId;
    }
    oss << '}';
  }
  oss << "}";
  return oss.str();
}

} // namespace nodo::node
