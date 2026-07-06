#include "node/FinalizedGovernanceAudit.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

core::LedgerRecord minimalRecord() {
  const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair(
      "governance-audit-test-key");
  core::Transaction tx(core::TransactionType::TRANSFER, kp.address().value(),
                       "governance-audit-recipient",
                       utils::Amount::fromRawUnits(100),
                       utils::Amount::fromRawUnits(10), 1, kTimestamp);
  const crypto::Ed25519SignatureProvider provider;
  tx.withChainId("nodo-localnet-1");
  tx.attachSignatureBundle(crypto::SignatureBundle::createSignature(
      tx.signingPayload(), kp.publicKey(), kp.privateKeyForSigningOnly(),
      kTimestamp, provider, crypto::SigningDomain::USER_TRANSACTION));
  return core::LedgerRecord::fromTransaction(
      tx, crypto::CryptoPolicy::developmentPolicy(),
      crypto::SecurityContext::USER_TRANSACTION, kTimestamp);
}

node::FinalizedBlockArtifact
makeArtifact(node::FinalizedTreasurySection treasurySection =
                 node::FinalizedTreasurySection{}) {
  return node::FinalizedBlockArtifact(
      core::Block::createGenesisBlock({minimalRecord()}, kTimestamp),
      "governance-audit-post-state-root", utils::Amount{},
      economics::SupplyDelta{}, {}, {}, {}, {}, {}, {}, {},
      node::MonetaryFirewallAudit{}, node::GenesisTreasurySnapshot{},
      node::ProtectionRewardBudget{}, {}, {}, node::ProtectionRewardSummary{},
      {}, node::InflationEpochSnapshot{}, node::MintAuthorizationRecord{},
      node::SupplyExpansionRecord{}, node::FeeEconomicBalance{},
      node::FeeBurnRecord{}, node::TreasuryFeeRecord{}, {}, {},
      node::SlashingEvidenceSummary{}, {}, {},
      node::CryptographicSlashingSummary{}, node::GovernancePolicySnapshot{},
      {}, node::GovernanceSummary{}, {}, node::EpochAccountingRecord{},
      node::ValidatorLifecycleSummary{}, consensus::QuorumCertificate{},
      consensus::FinalizedBlockRecord{}, std::move(treasurySection));
}

void testEmptyRuntimeAndArtifactsAccepted() {
  const node::GovernanceExecutor governance;
  const auto result =
      node::FinalizedGovernanceAudit::auditRuntime(governance, {}, 0);
  assert(result.accepted());
  assert(result.lifecycleCount() == 0);
  assert(result.evidenceCount() == 0);
}

void testLifecycleBackedTreasuryEvidenceAccepted() {
  const node::GovernanceExecutor governance;
  const economics::TreasuryExecutionEvidence evidence =
      tests::fixtures::validExecutionEvidence("ev-gov-001", "lifecycle-gov-001",
                                              "prop-gov-001", "gov-prop-001");
  const node::FinalizedTreasurySection section({evidence});
  const std::vector<node::FinalizedBlockArtifact> artifacts = {
      makeArtifact(section)};

  const auto result =
      node::FinalizedGovernanceAudit::auditRuntime(governance, artifacts, 0);
  assert(result.accepted());
  assert(result.evidenceCount() == 1);
}

void testTreasuryEvidenceWithoutGovernanceContextRejected() {
  const node::GovernanceExecutor governance;
  const economics::TreasuryProposal proposal =
      tests::fixtures::validTreasuryProposal("prop-no-context");
  const economics::TreasuryPolicy policy = tests::fixtures::validSpendPolicy();
  const economics::TreasuryAccount treasury = tests::fixtures::validTreasury();
  const economics::GovernanceLifecycleRecord lifecycle =
      tests::fixtures::validLifecycle("lifecycle-no-context", "prop-no-context",
                                      "gov-prop-no-context");
  const economics::TreasuryApproval approval =
      tests::fixtures::approvalFromLifecycle(lifecycle);

  const economics::TreasurySpendValidationResult spendResult =
      economics::TreasurySpendValidator::validateSpend(
          economics::DefenseModeState::INACTIVE,
          economics::DefenseModePolicy::defaultPolicy(), treasury, policy,
          proposal, approval, 10, utils::Amount::fromRawUnits(0));
  assert(spendResult.accepted());

  // Legacy constructor: structurally valid spend evidence, but no governance
  // context. Chain audit must reject it because spend records alone do not
  // prove authorization.
  const economics::TreasuryExecutionEvidence evidence(
      "ev-no-context", proposal, approval, policy, treasury, 10,
      utils::Amount::fromRawUnits(0), spendResult.spendRecord(), kTimestamp);
  assert(evidence.isValid());
  assert(!evidence.hasGovernanceContext());

  const node::FinalizedTreasurySection section({evidence});
  const std::vector<node::FinalizedBlockArtifact> artifacts = {
      makeArtifact(section)};

  const auto result =
      node::FinalizedGovernanceAudit::auditRuntime(governance, artifacts, 0);
  assert(!result.accepted());
  assert(result.status() == node::FinalizedGovernanceAuditStatus::
                                TREASURY_EXECUTION_EVIDENCE_MISSING_CONTEXT);
}

void testStatusToString() {
  assert(node::finalizedGovernanceAuditStatusToString(
             node::FinalizedGovernanceAuditStatus::ACCEPTED) == "ACCEPTED");
  assert(node::finalizedGovernanceAuditStatusToString(
             node::FinalizedGovernanceAuditStatus::
                 GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED) ==
         "GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED");
}

} // namespace

int main() {
  testEmptyRuntimeAndArtifactsAccepted();
  testLifecycleBackedTreasuryEvidenceAccepted();
  testTreasuryEvidenceWithoutGovernanceContextRejected();
  testStatusToString();
  return 0;
}
