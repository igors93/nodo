#include "node/FinalizedGovernanceAudit.hpp"

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleVerifier.hpp"
#include "node/Governance.hpp"
#include "node/GovernanceLifecycleRecordBuilder.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nodo::node {

std::string
finalizedGovernanceAuditStatusToString(FinalizedGovernanceAuditStatus status) {
  switch (status) {
  case FinalizedGovernanceAuditStatus::ACCEPTED:
    return "ACCEPTED";
  case FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_POLICY:
    return "INVALID_ARTIFACT_GOVERNANCE_POLICY";
  case FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_GUARD:
    return "INVALID_ARTIFACT_GOVERNANCE_GUARD";
  case FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_SUMMARY:
    return "INVALID_ARTIFACT_GOVERNANCE_SUMMARY";
  case FinalizedGovernanceAuditStatus::GOVERNANCE_SUMMARY_MISMATCH:
    return "GOVERNANCE_SUMMARY_MISMATCH";
  case FinalizedGovernanceAuditStatus::GOVERNANCE_LIFECYCLE_BUILD_FAILED:
    return "GOVERNANCE_LIFECYCLE_BUILD_FAILED";
  case FinalizedGovernanceAuditStatus::GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED:
    return "GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED";
  case FinalizedGovernanceAuditStatus::DUPLICATE_GOVERNANCE_LIFECYCLE:
    return "DUPLICATE_GOVERNANCE_LIFECYCLE";
  case FinalizedGovernanceAuditStatus::
      TREASURY_EXECUTION_EVIDENCE_MISSING_CONTEXT:
    return "TREASURY_EXECUTION_EVIDENCE_MISSING_CONTEXT";
  case FinalizedGovernanceAuditStatus::TREASURY_EXECUTION_LIFECYCLE_MISMATCH:
    return "TREASURY_EXECUTION_LIFECYCLE_MISMATCH";
  case FinalizedGovernanceAuditStatus::
      TREASURY_EXECUTION_EVIDENCE_MISSING_FOR_EXECUTED_PROPOSAL:
    return "TREASURY_EXECUTION_EVIDENCE_MISSING_FOR_EXECUTED_PROPOSAL";
  default:
    return "INVALID_ARTIFACT_GOVERNANCE_SUMMARY";
  }
}

FinalizedGovernanceAuditResult::FinalizedGovernanceAuditResult()
    : m_accepted(false),
      m_status(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_SUMMARY),
      m_reason("FinalizedGovernanceAuditResult: uninitialized."),
      m_failedBlockHeight(0), m_proposalId(""), m_artifactCount(0),
      m_lifecycleCount(0), m_evidenceCount(0) {}

FinalizedGovernanceAuditResult
FinalizedGovernanceAuditResult::accepted(std::size_t artifactCount,
                                         std::size_t lifecycleCount,
                                         std::size_t evidenceCount) {
  FinalizedGovernanceAuditResult result;
  result.m_accepted = true;
  result.m_status = FinalizedGovernanceAuditStatus::ACCEPTED;
  result.m_reason = "";
  result.m_artifactCount = artifactCount;
  result.m_lifecycleCount = lifecycleCount;
  result.m_evidenceCount = evidenceCount;
  return result;
}

FinalizedGovernanceAuditResult FinalizedGovernanceAuditResult::rejected(
    FinalizedGovernanceAuditStatus status, std::string reason,
    std::uint64_t failedBlockHeight, std::string proposalId,
    std::size_t lifecycleCount, std::size_t evidenceCount) {
  FinalizedGovernanceAuditResult result;
  result.m_accepted = false;
  result.m_status = status;
  result.m_reason = std::move(reason);
  result.m_failedBlockHeight = failedBlockHeight;
  result.m_proposalId = std::move(proposalId);
  result.m_lifecycleCount = lifecycleCount;
  result.m_evidenceCount = evidenceCount;
  return result;
}

bool FinalizedGovernanceAuditResult::accepted() const { return m_accepted; }
FinalizedGovernanceAuditStatus FinalizedGovernanceAuditResult::status() const {
  return m_status;
}
const std::string &FinalizedGovernanceAuditResult::reason() const {
  return m_reason;
}
std::uint64_t FinalizedGovernanceAuditResult::failedBlockHeight() const {
  return m_failedBlockHeight;
}
const std::string &FinalizedGovernanceAuditResult::proposalId() const {
  return m_proposalId;
}
std::size_t FinalizedGovernanceAuditResult::artifactCount() const {
  return m_artifactCount;
}
std::size_t FinalizedGovernanceAuditResult::lifecycleCount() const {
  return m_lifecycleCount;
}
std::size_t FinalizedGovernanceAuditResult::evidenceCount() const {
  return m_evidenceCount;
}

std::string FinalizedGovernanceAuditResult::serialize() const {
  std::ostringstream oss;
  oss << "FinalizedGovernanceAuditResult{"
      << "accepted=" << (m_accepted ? "1" : "0")
      << ";status=" << finalizedGovernanceAuditStatusToString(m_status)
      << ";reason=" << m_reason << ";failedBlockHeight=" << m_failedBlockHeight
      << ";proposalId=" << m_proposalId << ";artifactCount=" << m_artifactCount
      << ";lifecycleCount=" << m_lifecycleCount
      << ";evidenceCount=" << m_evidenceCount << "}";
  return oss.str();
}

namespace {

bool hasActiveGovernanceSection(const FinalizedBlockArtifact &artifact) {
  return artifact.governancePolicySnapshot().active() ||
         !artifact.governanceActionGuards().empty() ||
         artifact.governanceSummary().active();
}

FinalizedGovernanceAuditResult auditArtifactGovernanceSections(
    const std::vector<FinalizedBlockArtifact> &artifacts) {
  for (const auto &artifact : artifacts) {
    if (!hasActiveGovernanceSection(artifact)) {
      continue;
    }

    const std::uint64_t height = artifact.block().index();
    if (!artifact.governancePolicySnapshot().isValid() ||
        !artifact.governancePolicySnapshot().active()) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_POLICY,
          "chain audit: finalized artifact carries an inactive or invalid "
          "governance policy snapshot at block " +
              std::to_string(height),
          height);
    }

    GovernancePolicySnapshot expectedPolicy;
    std::vector<GovernanceActionGuard> expectedGuards;
    try {
      expectedPolicy = Governance::buildPolicySnapshot(height);
      expectedGuards = Governance::buildActionGuards(expectedPolicy);
    } catch (const std::exception &e) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_POLICY,
          std::string("chain audit: failed to rebuild governance policy for ") +
              "block " + std::to_string(height) + ": " + e.what(),
          height);
    }

    if (!Governance::samePolicy(expectedPolicy,
                                artifact.governancePolicySnapshot())) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_POLICY,
          "chain audit: governance policy snapshot mismatch at block " +
              std::to_string(height),
          height);
    }

    if (!Governance::sameActionGuards(expectedGuards,
                                      artifact.governanceActionGuards())) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_GUARD,
          "chain audit: governance action guards mismatch at block " +
              std::to_string(height),
          height);
    }

    const GovernanceSummary &summary = artifact.governanceSummary();
    if (!summary.isValid() || !summary.active()) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_SUMMARY,
          "chain audit: finalized artifact carries an inactive or invalid "
          "governance summary at block " +
              std::to_string(height),
          height);
    }

    if (summary.blockHeight() != height ||
        summary.guardCount() != expectedGuards.size() ||
        summary.reason() != Governance::SUMMARY_REASON ||
        summary.sourceGuardDigest().empty()) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::INVALID_ARTIFACT_GOVERNANCE_SUMMARY,
          "chain audit: governance summary is not structurally consistent at "
          "block " +
              std::to_string(height),
          height);
    }
  }

  return FinalizedGovernanceAuditResult::accepted(artifacts.size(), 0, 0);
}

bool statusHasDecidedLifecycle(GovernanceProposalStatus status) {
  return status == GovernanceProposalStatus::REJECTED ||
         status == GovernanceProposalStatus::QUEUED_FOR_EXECUTION ||
         status == GovernanceProposalStatus::EXECUTED ||
         status == GovernanceProposalStatus::FAILED_EXECUTION;
}

bool sameLifecycleIdentity(
    const economics::GovernanceLifecycleRecord &lifecycle,
    const GovernanceExecutor::GovernanceProposalSnapshot &snapshot) {
  return lifecycle.lifecycleId() == snapshot.proposalId &&
         lifecycle.proposalEnvelope().governanceProposalId() ==
             snapshot.proposalId &&
         lifecycle.decisionRecord().governanceProposalId() ==
             snapshot.proposalId &&
         lifecycle.createdAtBlock() == snapshot.createdHeight &&
         lifecycle.finalizedAtBlock() == snapshot.decidedAtHeight;
}

std::map<std::string, std::uint64_t> artifactHeightsByExecutedProposal(
    const std::vector<FinalizedBlockArtifact> &artifacts) {
  std::map<std::string, std::uint64_t> result;
  for (const auto &artifact : artifacts) {
    for (const auto &evidence :
         artifact.treasurySection().executionEvidence()) {
      if (evidence.isValid() && evidence.spendRecord().isValid()) {
        result[evidence.spendRecord().proposalId()] = artifact.block().index();
      }
    }
  }
  return result;
}

std::set<std::uint64_t>
loadedArtifactHeights(const std::vector<FinalizedBlockArtifact> &artifacts) {
  std::set<std::uint64_t> heights;
  for (const auto &artifact : artifacts) {
    heights.insert(artifact.block().index());
  }
  return heights;
}

} // namespace

FinalizedGovernanceAuditResult FinalizedGovernanceAudit::auditRuntime(
    const GovernanceExecutor &governance,
    const std::vector<FinalizedBlockArtifact> &artifacts,
    std::uint64_t auditedTipHeight) {
  const FinalizedGovernanceAuditResult artifactAudit =
      auditArtifactGovernanceSections(artifacts);
  if (!artifactAudit.accepted()) {
    return artifactAudit;
  }

  std::size_t lifecycleCount = 0;
  std::size_t evidenceCount = 0;
  std::set<std::string> lifecycleIds;
  std::map<std::string, economics::GovernanceLifecycleRecord>
      verifiedRuntimeLifecycles;
  const std::map<std::string, std::uint64_t> evidenceHeights =
      artifactHeightsByExecutedProposal(artifacts);
  const std::set<std::uint64_t> artifactHeights =
      loadedArtifactHeights(artifacts);

  for (const std::string &proposalId : governance.proposalIds()) {
    const GovernanceExecutor::GovernanceProposalSnapshot snapshot =
        governance.proposalSnapshot(proposalId);
    if (snapshot.proposalId.empty()) {
      continue;
    }

    if (snapshot.payload.type() !=
            core::GovernanceProposalType::TREASURY_SPEND ||
        snapshot.decidedAtHeight == 0 ||
        !statusHasDecidedLifecycle(snapshot.status)) {
      continue;
    }

    std::optional<economics::GovernanceLifecycleRecord> lifecycle;
    try {
      lifecycle = GovernanceLifecycleRecordBuilder::buildDecided(snapshot);
    } catch (const std::exception &e) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::GOVERNANCE_LIFECYCLE_BUILD_FAILED,
          std::string(
              "chain audit: failed to rebuild governance lifecycle for ") +
              "proposal " + proposalId + ": " + e.what(),
          snapshot.decidedAtHeight, proposalId, lifecycleCount, evidenceCount);
    }

    if (!lifecycle.has_value()) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::GOVERNANCE_LIFECYCLE_BUILD_FAILED,
          "chain audit: decided treasury proposal did not produce a lifecycle "
          "record: " +
              proposalId,
          snapshot.decidedAtHeight, proposalId, lifecycleCount, evidenceCount);
    }

    if (!lifecycleIds.insert(lifecycle->lifecycleId()).second) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::DUPLICATE_GOVERNANCE_LIFECYCLE,
          "chain audit: duplicate governance lifecycle id: " +
              lifecycle->lifecycleId(),
          snapshot.decidedAtHeight, proposalId, lifecycleCount, evidenceCount);
    }

    const economics::GovernanceLifecycleVerificationResult verification =
        economics::GovernanceLifecycleVerifier::verify(*lifecycle);
    if (!verification.verified()) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::
              GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED,
          "chain audit: rebuilt governance lifecycle failed verification for "
          "proposal " +
              proposalId + ": " + verification.reason(),
          snapshot.decidedAtHeight, proposalId, lifecycleCount, evidenceCount);
    }

    if (!sameLifecycleIdentity(*lifecycle, snapshot)) {
      return FinalizedGovernanceAuditResult::rejected(
          FinalizedGovernanceAuditStatus::
              GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED,
          "chain audit: rebuilt governance lifecycle identity does not match "
          "runtime proposal snapshot: " +
              proposalId,
          snapshot.decidedAtHeight, proposalId, lifecycleCount, evidenceCount);
    }

    verifiedRuntimeLifecycles.emplace(proposalId, *lifecycle);
    lifecycleCount++;

    if (snapshot.status == GovernanceProposalStatus::EXECUTED &&
        snapshot.executedAtHeight > 0) {
      const bool executionArtifactLoaded =
          artifactHeights.empty() ||
          artifactHeights.count(snapshot.executedAtHeight) > 0;
      const bool evidencePresent = evidenceHeights.count(proposalId) > 0;
      if (executionArtifactLoaded && !evidencePresent) {
        return FinalizedGovernanceAuditResult::rejected(
            FinalizedGovernanceAuditStatus::
                TREASURY_EXECUTION_EVIDENCE_MISSING_FOR_EXECUTED_PROPOSAL,
            "chain audit: executed treasury proposal has no embedded execution "
            "evidence in its loaded finalized artifact: " +
                proposalId,
            snapshot.executedAtHeight, proposalId, lifecycleCount,
            evidenceCount);
      }
    }
  }

  for (const auto &artifact : artifacts) {
    for (const auto &evidence :
         artifact.treasurySection().executionEvidence()) {
      evidenceCount++;
      if (!evidence.hasGovernanceContext()) {
        return FinalizedGovernanceAuditResult::rejected(
            FinalizedGovernanceAuditStatus::
                TREASURY_EXECUTION_EVIDENCE_MISSING_CONTEXT,
            "chain audit: treasury execution evidence is missing governance "
            "lifecycle context at block " +
                std::to_string(artifact.block().index()),
            artifact.block().index(), evidence.spendRecord().proposalId(),
            lifecycleCount, evidenceCount);
      }

      const economics::GovernanceLifecycleRecord &embeddedLifecycle =
          evidence.governanceContext().governanceLifecycle;
      const economics::GovernanceLifecycleVerificationResult verification =
          economics::GovernanceLifecycleVerifier::verify(embeddedLifecycle);
      if (!verification.verified()) {
        return FinalizedGovernanceAuditResult::rejected(
            FinalizedGovernanceAuditStatus::
                GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED,
            "chain audit: embedded treasury governance lifecycle failed "
            "verification at block " +
                std::to_string(artifact.block().index()) + ": " +
                verification.reason(),
            artifact.block().index(), evidence.spendRecord().proposalId(),
            lifecycleCount, evidenceCount);
      }

      const std::string proposalId = evidence.spendRecord().proposalId();
      const auto runtimeIt = verifiedRuntimeLifecycles.find(proposalId);
      if (runtimeIt != verifiedRuntimeLifecycles.end() &&
          embeddedLifecycle.serialize() != runtimeIt->second.serialize()) {
        return FinalizedGovernanceAuditResult::rejected(
            FinalizedGovernanceAuditStatus::
                TREASURY_EXECUTION_LIFECYCLE_MISMATCH,
            "chain audit: embedded treasury governance lifecycle does not "
            "match "
            "the lifecycle rebuilt from runtime state for proposal " +
                proposalId,
            artifact.block().index(), proposalId, lifecycleCount,
            evidenceCount);
      }
    }
  }

  if (!artifacts.empty()) {
    const FinalizedBlockArtifact &latestArtifact = artifacts.back();
    if (latestArtifact.block().index() == auditedTipHeight &&
        latestArtifact.governanceSummary().active()) {
      try {
        const GovernancePolicySnapshot expectedPolicy =
            Governance::buildPolicySnapshot(auditedTipHeight);
        const std::vector<GovernanceActionGuard> expectedGuards =
            Governance::buildActionGuards(expectedPolicy);
        const GovernanceSummary expectedSummary = Governance::buildSummary(
            auditedTipHeight, expectedGuards,
            static_cast<std::uint64_t>(governance.activeProposalCount()),
            static_cast<std::uint64_t>(governance.approvedProposalCount()),
            static_cast<std::uint64_t>(
                governance.executableProposalCount(auditedTipHeight + 1)),
            static_cast<std::uint64_t>(governance.executedProposalCount()),
            governance.serialize());

        if (!Governance::sameSummary(expectedSummary,
                                     latestArtifact.governanceSummary())) {
          return FinalizedGovernanceAuditResult::rejected(
              FinalizedGovernanceAuditStatus::GOVERNANCE_SUMMARY_MISMATCH,
              "chain audit: latest governance summary does not match rebuilt "
              "runtime governance state at height " +
                  std::to_string(auditedTipHeight),
              auditedTipHeight, "", lifecycleCount, evidenceCount);
        }
      } catch (const std::exception &e) {
        return FinalizedGovernanceAuditResult::rejected(
            FinalizedGovernanceAuditStatus::GOVERNANCE_SUMMARY_MISMATCH,
            std::string("chain audit: failed to rebuild latest governance ") +
                "summary: " + e.what(),
            auditedTipHeight, "", lifecycleCount, evidenceCount);
      }
    }
  }

  return FinalizedGovernanceAuditResult::accepted(
      artifacts.size(), lifecycleCount, evidenceCount);
}

} // namespace nodo::node
