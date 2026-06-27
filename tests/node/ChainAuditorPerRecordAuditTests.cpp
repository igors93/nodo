#include "config/GenesisRegistry.hpp"
#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "economics/BurnRecord.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/ChainAuditor.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Build a minimal signed transaction and ledger record. The content is
// irrelevant to the per-record audit checks — it just satisfies Block's
// requirement of at least one record.
core::LedgerRecord minimalRecord() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicEd25519KeyPair("audit-test-key");
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        kp.address().value(),
        "audit-test-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        1,
        kTimestamp
    );
    const crypto::Ed25519SignatureProvider provider;
    tx.withChainId("nodo-localnet-1");
    tx.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            tx.signingPayload(),
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            kTimestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

// Build a runtime that has one finalized SupplyDelta so the per-record checks
// inside ChainAuditor::auditImpl are actually reached.  Without at least one
// finalized delta, the entire per-record section is skipped.
node::RuntimeStateLoadResult runtimeWithOneDelta(
    const std::vector<node::FinalizedBlockArtifact>& artifacts = {}
) {
    const config::GenesisConfig genesis =
        config::GenesisRegistry::get("localnet").genesis();

    const node::NodeRuntimeStartResult start =
        node::NodeRuntimeFactory::startFromGenesis(
            node::NodeRuntimeConfig(
                genesis,
                p2p::PeerInfo(
                    "chain-audit-perrecord-peer",
                    "127.0.0.1:9002",
                    "nodo/0.1",
                    0,
                    kTimestamp
                ),
                genesis.networkParameters().maxPeerCount()
            )
        );

    if (!start.started()) {
        throw std::runtime_error(
            "runtimeWithOneDelta: failed to start runtime: " + start.reason()
        );
    }

    node::NodeRuntime rt = start.runtime();

    const utils::Amount genesisSupply =
        node::MonetaryFirewall::genesisSupply(genesis);

    const economics::BurnRecord burn(
        "per-record-audit-test", 1, 0, "fee_pool",
        utils::Amount::fromRawUnits(100),
        "fee burn",
        economics::BurnType::FEE_BURN
    );

    // Minimal valid SupplyDelta: supplyBefore == genesisSupply so the
    // FinalizedSupplyAudit continuity check passes.
    const economics::SupplyDelta delta(
        1,
        "test-block-hash-per-record",
        0,
        genesisSupply,
        utils::Amount::fromRawUnits(0),
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(genesisSupply.rawUnits() - 100),
        {},
        {burn}
    );

    rt.mutableSupplyState().applyFinalizedDelta(delta);

    const node::NodeRuntimeManifest manifest =
        node::NodeRuntimeManifest::fromRuntime(rt, kTimestamp, kTimestamp);

    return node::RuntimeStateLoadResult::loaded(rt, manifest, 0, 1, artifacts);
}

// Construct a minimal FinalizedBlockArtifact using only the fields that each
// per-record test actually cares about.  Everything else is left at its
// default / empty state so that the earlier audit stages (reward evidence,
// treasury section) pass without needing a full production artifact.
node::FinalizedBlockArtifact makeArtifact(
    std::vector<node::SlashingEvidenceRecord> slashingRecords = {},
    node::SlashingEvidenceSummary slashingSummary =
        node::SlashingEvidenceSummary::notEvaluated(),
    std::vector<node::ValidatorLifecycleRecord> lifecycleRecords = {},
    node::ValidatorLifecycleSummary lifecycleSummary =
        node::ValidatorLifecycleSummary::notEvaluated(),
    std::vector<node::RewardDistribution> rewardDistributions = {}
) {
    return node::FinalizedBlockArtifact(
        // block: genesis block with one record (Block requires at least one)
        core::Block::createGenesisBlock({minimalRecord()}, kTimestamp),
        "artifact-post-state-root",
        utils::Amount{},
        economics::SupplyDelta{},
        std::move(rewardDistributions),          // RewardDistribution (check D target)
        {},                                      // lockedStakePositions
        {},                                      // securityScoreRecords
        {},                                      // securityCheckpoints
        {},                                      // validatorRiskAssessments
        {},                                      // validatorContainmentDecisions
        {},                                      // validatorNetworkPolicies
        node::MonetaryFirewallAudit{},
        node::GenesisTreasurySnapshot{},
        node::ProtectionRewardBudget{},
        {},                                      // protectionRewardGrants
        {},                                      // protectionWorkRecords
        node::ProtectionRewardSummary{},
        {},                                      // protectionRewardSettlements — EMPTY so
                                                 // reward evidence audit passes trivially
        node::InflationEpochSnapshot{},
        node::MintAuthorizationRecord{},
        node::SupplyExpansionRecord{},
        node::FeeEconomicBalance{},
        node::FeeBurnRecord{},
        node::TreasuryFeeRecord{},
        std::move(slashingRecords),              // SlashingEvidenceRecord (check A/B target)
        {},                                      // slashingPreparationRecords
        std::move(slashingSummary),              // SlashingEvidenceSummary (check B target)
        {},                                      // cryptographicSlashingEvidenceRecords
        {},                                      // stakePenaltyRecords
        node::CryptographicSlashingSummary{},
        node::GovernancePolicySnapshot{},
        {},                                      // governanceActionGuards
        node::GovernanceSummary{},
        std::move(lifecycleRecords),             // ValidatorLifecycleRecord (check C target)
        node::EpochAccountingRecord::notEvaluated(),
        std::move(lifecycleSummary),             // ValidatorLifecycleSummary (check C target)
        consensus::QuorumCertificate{},
        consensus::FinalizedBlockRecord{},
        node::FinalizedTreasurySection{}         // empty treasury section — always valid
    );
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Regression guard: a runtime that has finalized deltas but artifacts with no
// defects must still pass all four per-record checks.
void testPerRecordChecksPassOnCleanArtifacts() {
    const std::vector<node::FinalizedBlockArtifact> artifacts = {
        makeArtifact()
    };

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(
            runtimeWithOneDelta(artifacts)
        );

    requireCondition(
        result.passed(),
        "Clean artifact with no defects must pass per-record audit checks."
    );
}

// Check A: the same (validatorAddress, blockHeight) pair must not appear in
// more than one artifact — that would mean the same misbehaviour was slashed
// twice.
void testDuplicateSlashEvidenceAcrossArtifactsIsRejected() {
    const node::SlashingEvidenceRecord duplicate(
        "validator-slash",
        42,
        "double-vote",
        500,
        true,
        "SLASH",
        node::SlashingEvidence::RISK_CONTAINMENT_EVIDENCE_REASON,
        "evidence-digest-001"
    );

    const std::vector<node::FinalizedBlockArtifact> artifacts = {
        makeArtifact({duplicate}),
        makeArtifact({duplicate})   // same validator + height in a second artifact
    };

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(
            runtimeWithOneDelta(artifacts)
        );

    requireCondition(
        !result.passed(),
        "Duplicate slash evidence across artifacts must be rejected."
    );
    requireCondition(
        result.reason().find("duplicate") != std::string::npos,
        "Rejection reason must mention 'duplicate'."
    );
}

// Check B: the summary's evidenceCount must equal the number of actual
// SlashingEvidenceRecord entries in the same artifact.
void testSlashEvidenceSummaryCountMismatchIsRejected() {
    // Summary claims 3 evidence records but the artifact contains only 1.
    const node::SlashingEvidenceSummary activeSummary(
        "ACTIVE",
        1,
        3,   // evidenceCount — mismatch: only 1 actual record below
        0,   // slashableEvidenceCount (≤ evidenceCount)
        0,   // maxSeverityScore (≤ 1000)
        utils::Amount::fromRawUnits(0),
        node::SlashingEvidence::SLASHING_SUMMARY_REASON,
        "prep-digest"   // non-empty required when evidenceCount > 0
    );

    const node::SlashingEvidenceRecord oneRecord(
        "validator-b",
        10,
        "liveness-failure",
        100,
        false,
        "WARN",
        node::SlashingEvidence::RISK_CONTAINMENT_EVIDENCE_REASON,
        "evidence-digest-b"
    );

    const std::vector<node::FinalizedBlockArtifact> artifacts = {
        makeArtifact({oneRecord}, activeSummary)
    };

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(
            runtimeWithOneDelta(artifacts)
        );

    requireCondition(
        !result.passed(),
        "Slash evidence summary count mismatch must be rejected."
    );
    requireCondition(
        result.reason().find("mismatch") != std::string::npos ||
        result.reason().find("summary") != std::string::npos,
        "Rejection reason must mention 'mismatch' or 'summary'."
    );
}

// Check C: active + jailed + slashed from the ValidatorLifecycleSummary must
// equal the number of ValidatorLifecycleRecord entries in the same artifact.
void testValidatorLifecycleSummaryCountMismatchIsRejected() {
    // Summary claims 2 active validators but the artifact has no lifecycle
    // records at all.
    const node::ValidatorLifecycleSummary activeSummary(
        "ACTIVE",
        1,
        0,                           // epochIndex
        2,                           // activeValidatorCount  ┐ total = 2
        0,                           // jailedValidatorCount  │
        0,                           // slashedValidatorCount ┘
        utils::Amount::fromRawUnits(0),
        utils::Amount::fromRawUnits(0),
        utils::Amount::fromRawUnits(0),
        node::ValidatorLifecycle::SUMMARY_REASON,
        "epoch-digest"               // non-empty required for active summary
    );

    const std::vector<node::FinalizedBlockArtifact> artifacts = {
        makeArtifact({}, node::SlashingEvidenceSummary::notEvaluated(),
                     {},             // validatorLifecycleRecords — empty (size 0)
                     activeSummary)  // but summary claims total = 2
    };

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(
            runtimeWithOneDelta(artifacts)
        );

    requireCondition(
        !result.passed(),
        "Validator lifecycle summary count mismatch must be rejected."
    );
    requireCondition(
        result.reason().find("lifecycle") != std::string::npos ||
        result.reason().find("mismatch") != std::string::npos,
        "Rejection reason must mention 'lifecycle' or 'mismatch'."
    );
}

// Check D: every RewardDistribution in an artifact must satisfy its own
// isValid() contract.  An amount split where liquidReward + lockedReward
// differs from totalReward is structurally invalid.
void testInvalidRewardDistributionIsRejected() {
    // liquidReward (60) + lockedReward (50) = 110 ≠ totalReward (100).
    const node::RewardDistribution badDist(
        "validator-reward",
        1,
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(60),
        utils::Amount::fromRawUnits(50),
        node::RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON
    );

    const std::vector<node::FinalizedBlockArtifact> artifacts = {
        makeArtifact({}, node::SlashingEvidenceSummary::notEvaluated(),
                     {}, node::ValidatorLifecycleSummary::notEvaluated(),
                     {badDist})
    };

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(
            runtimeWithOneDelta(artifacts)
        );

    requireCondition(
        !result.passed(),
        "Structurally invalid reward distribution must be rejected."
    );
    requireCondition(
        result.reason().find("reward") != std::string::npos ||
        result.reason().find("distribution") != std::string::npos ||
        result.reason().find("invalid") != std::string::npos,
        "Rejection reason must mention 'reward', 'distribution', or 'invalid'."
    );
}

} // namespace

int main() {
    try {
        testPerRecordChecksPassOnCleanArtifacts();
        testDuplicateSlashEvidenceAcrossArtifactsIsRejected();
        testSlashEvidenceSummaryCountMismatchIsRejected();
        testValidatorLifecycleSummaryCountMismatchIsRejected();
        testInvalidRewardDistributionIsRejected();

        std::cout << "ChainAuditor per-record audit tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ChainAuditor per-record audit tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
