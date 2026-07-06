#include "node/GovernanceLifecycleRecordBuilder.hpp"

#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceTransitionProof.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/GovernanceVoteSetAudit.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <stdexcept>

namespace nodo::node {

namespace {

// Every lifecycle record produced by this builder uses the same fixed policy
// identity: the policy itself is not governance-configurable (see the
// timelock/spend-limit ticket scope), it only needs to be internally
// consistent so economics::GovernanceLifecycleVerifier can replay it.
constexpr const char *kPolicyVersion = "nodo-treasury-governance-v1";
constexpr const char *kSystemActor = "nodo-governance-executor";

// KeyValueFileCodec-backed records elsewhere in this codebase reject empty
// field values; an empty transition reason is otherwise meaningless here, so
// use the same "no reason recorded" placeholder.
constexpr const char *kNoTransitionReason = "-";

// GovernanceExecutor decides quorum/approval with exact-fraction arithmetic
// (participating*denominator >= total*numerator); economics::
// GovernanceVotingPolicy expresses the same idea as an absolute voting-power
// threshold plus approval basis points (parts per 10000). Fractions such as
// 2/3 have no exact basis-points representation, so the two engines cannot
// share a formula in general. Since this proposal is already decided, the
// policy only needs to reproduce GovernanceExecutor's own quorumMet/
// approvalThresholdMet booleans for THIS SPECIFIC tally, not for tallies in
// general — so it is derived from the outcome rather than from the fraction.
economics::GovernanceVotingPolicy votingPolicyFor(
    const GovernanceExecutor::GovernanceProposalSnapshot &snapshot) {
  const auto &tally = snapshot.tally;
  const std::uint64_t participating = tally.participatingWeight();
  const std::uint64_t yes = tally.yesWeight();
  const std::uint64_t directional = yes + tally.noWeight();

  const std::uint64_t quorumVotingPower =
      tally.quorumMet() ? participating : participating + 1;

  std::uint32_t approvalBasisPoints = 0;
  if (directional > 0) {
    const std::uint64_t achievedBasisPoints = (yes * 10000) / directional;
    approvalBasisPoints = static_cast<std::uint32_t>(
        tally.approvalThresholdMet() ? achievedBasisPoints
                                     : achievedBasisPoints + 1);
  }

  return economics::GovernanceVotingPolicy(
      kPolicyVersion,
      utils::Amount::fromRawUnits(static_cast<std::int64_t>(quorumVotingPower)),
      approvalBasisPoints, utils::Amount::fromRawUnits(0), true, false);
}

economics::GovernanceLifecycleState
declaredStateFor(GovernanceProposalStatus status) {
  switch (status) {
  case GovernanceProposalStatus::REJECTED:
    return economics::GovernanceLifecycleState::DECIDED_REJECTED;
  case GovernanceProposalStatus::QUEUED_FOR_EXECUTION:
  case GovernanceProposalStatus::EXECUTED:
  case GovernanceProposalStatus::FAILED_EXECUTION:
    return economics::GovernanceLifecycleState::DECIDED_APPROVED;
  default:
    return economics::GovernanceLifecycleState::DRAFT;
  }
}

} // namespace

std::optional<economics::GovernanceLifecycleRecord>
GovernanceLifecycleRecordBuilder::buildDecided(
    const GovernanceExecutor::GovernanceProposalSnapshot &snapshot) {
  if (snapshot.proposalId.empty() ||
      snapshot.payload.type() != core::GovernanceProposalType::TREASURY_SPEND ||
      snapshot.decidedAtHeight == 0) {
    return std::nullopt;
  }

  const economics::GovernanceLifecycleState declaredState =
      declaredStateFor(snapshot.status);
  if (declaredState != economics::GovernanceLifecycleState::DECIDED_REJECTED &&
      declaredState != economics::GovernanceLifecycleState::DECIDED_APPROVED) {
    // Expired for lack of quorum, or not yet decided: GovernanceDecisionBuilder
    // can only express APPROVED/REJECTED, so there is nothing to build.
    return std::nullopt;
  }

  const economics::TreasuryProposal treasuryProposal(
      snapshot.proposalId, snapshot.payload.treasuryRecipient(),
      utils::Amount::fromRawUnits(snapshot.payload.treasuryAmountRaw()),
      snapshot.payload.title(), snapshot.decidedAtHeight,
      ValidatorLifecycle::epochIndexForBlock(snapshot.decidedAtHeight),
      snapshot.proposerAddress);

  const economics::GovernanceProposalEnvelope envelope(
      snapshot.proposalId,
      core::governanceProposalTypeToString(snapshot.payload.type()),
      treasuryProposal, snapshot.createdHeight, snapshot.proposerAddress,
      kPolicyVersion, snapshot.proposalId);

  const economics::GovernanceVotingPolicy votingPolicy =
      votingPolicyFor(snapshot);

  std::vector<economics::GovernanceVoteEvidence> voteEvidenceList;
  voteEvidenceList.reserve(snapshot.votes.size());
  for (const auto &vote : snapshot.votes) {
    const core::GovernanceVoteChoice coreChoice = vote.choice;
    const economics::GovernanceVoteChoice choice =
        coreChoice == core::GovernanceVoteChoice::YES
            ? economics::GovernanceVoteChoice::YES
        : coreChoice == core::GovernanceVoteChoice::ABSTAIN
            ? economics::GovernanceVoteChoice::ABSTAIN
            : economics::GovernanceVoteChoice::NO;
    const utils::Amount power =
        utils::Amount::fromRawUnits(static_cast<std::int64_t>(vote.weight));

    const std::string proof = economics::GovernanceVoteProof::build(
        snapshot.proposalId, vote.validatorAddress, choice, power,
        vote.castHeight, kPolicyVersion);
    economics::GovernanceVoteRecord record(
        vote.transactionId, snapshot.proposalId, vote.validatorAddress, choice,
        power, vote.castHeight, "validator-stake", proof, kPolicyVersion);
    voteEvidenceList.emplace_back("evidence-" + vote.transactionId, envelope,
                                  votingPolicy, std::move(record));
  }

  const economics::GovernanceVoteSetAuditResult voteAudit =
      economics::GovernanceVoteSetAudit::audit(snapshot.proposalId,
                                               votingPolicy, voteEvidenceList);
  if (!voteAudit.accepted()) {
    throw std::logic_error("GovernanceLifecycleRecordBuilder: vote-set audit "
                           "rejected votes derived "
                           "from GovernanceExecutor's own state: " +
                           voteAudit.reason());
  }

  const economics::GovernanceTallyReport tallyReport =
      economics::GovernanceTallyReport::build(voteAudit, votingPolicy);
  if (!tallyReport.isValid()) {
    throw std::logic_error("GovernanceLifecycleRecordBuilder: recomputed tally "
                           "report is invalid: " +
                           tallyReport.rejectionReason());
  }

  const economics::GovernancePolicy governancePolicy(kPolicyVersion, 0, 0, true,
                                                     false);

  const economics::GovernanceDecisionBuildResult decisionBuild =
      economics::GovernanceDecisionBuilder::buildDecision(
          envelope, votingPolicy, tallyReport, snapshot.decidedAtHeight,
          kSystemActor);
  if (!decisionBuild.built()) {
    throw std::logic_error(
        "GovernanceLifecycleRecordBuilder: decision builder rejected an "
        "already-decided proposal: " +
        decisionBuild.reason());
  }

  using S = economics::GovernanceLifecycleState;
  auto makeTransition = [&](const std::string &suffix, S from, S to,
                            std::uint64_t block, const std::string &actor) {
    const std::string id = snapshot.proposalId + suffix;
    const std::string proof = economics::GovernanceTransitionProof::build(
        snapshot.proposalId, id, from, to, block, actor, kPolicyVersion);
    return economics::GovernanceLifecycleTransition(
        id, snapshot.proposalId, from, to, block, actor, kNoTransitionReason,
        proof, kPolicyVersion);
  };

  std::vector<economics::GovernanceLifecycleTransition> transitions{
      makeTransition("-submitted", S::DRAFT, S::SUBMITTED,
                     snapshot.createdHeight, snapshot.proposerAddress),
      makeTransition("-review", S::SUBMITTED, S::REVIEW, snapshot.createdHeight,
                     kSystemActor),
      makeTransition("-voting", S::REVIEW, S::VOTING,
                     snapshot.votingStartHeight, kSystemActor),
      makeTransition("-tallying", S::VOTING, S::TALLYING,
                     snapshot.votingEndHeight, kSystemActor),
      makeTransition("-decision", S::TALLYING, declaredState,
                     snapshot.decidedAtHeight, kSystemActor),
  };

  economics::GovernanceLifecycleRecord lifecycle(
      snapshot.proposalId, envelope, governancePolicy, votingPolicy,
      std::move(voteEvidenceList), tallyReport, decisionBuild.decisionRecord(),
      snapshot.createdHeight, snapshot.decidedAtHeight, declaredState,
      std::move(transitions));

  if (!lifecycle.isValid()) {
    throw std::logic_error("GovernanceLifecycleRecordBuilder: built lifecycle "
                           "record is invalid: " +
                           lifecycle.rejectionReason());
  }

  return lifecycle;
}

economics::GovernanceLifecycleRecord
GovernanceLifecycleRecordBuilder::buildExecuted(
    const economics::GovernanceLifecycleRecord &decided,
    std::uint64_t executedAtHeight) {
  using S = economics::GovernanceLifecycleState;
  const std::string &proposalId =
      decided.proposalEnvelope().governanceProposalId();
  const std::string &policyVersion = decided.governancePolicy().policyVersion();

  auto makeTransition = [&](const std::string &suffix, S from, S to) {
    const std::string id = proposalId + suffix;
    const std::string proof = economics::GovernanceTransitionProof::build(
        proposalId, id, from, to, executedAtHeight, kSystemActor,
        policyVersion);
    return economics::GovernanceLifecycleTransition(
        id, proposalId, from, to, executedAtHeight, kSystemActor,
        kNoTransitionReason, proof, policyVersion);
  };

  std::vector<economics::GovernanceLifecycleTransition> transitions =
      decided.transitionHistory();
  transitions.push_back(makeTransition(
      "-approval-produced", S::DECIDED_APPROVED, S::APPROVAL_PRODUCED));
  transitions.push_back(
      makeTransition("-executed", S::APPROVAL_PRODUCED, S::EXECUTED));

  return economics::GovernanceLifecycleRecord(
      decided.lifecycleId(), decided.proposalEnvelope(),
      decided.governancePolicy(), decided.votingPolicy(),
      decided.voteEvidenceList(), decided.tallyReport(),
      decided.decisionRecord(), decided.createdAtBlock(), executedAtHeight,
      S::EXECUTED, std::move(transitions));
}

} // namespace nodo::node
