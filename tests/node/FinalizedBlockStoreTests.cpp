#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedArtifactSchema.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::crypto::SignatureBundle;
using nodo::node::FinalizedBlockStore;
using nodo::node::FinalizedArtifactSchema;
using nodo::node::FinalizedBlockArtifactCodec;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-finalized-block-store-tests-" + suffix);
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(
        path,
        error
    );
}

std::string replaceFirst(
    std::string contents,
    const std::string& from,
    const std::string& to
) {
    const std::size_t position =
        contents.find(from);

    requireCondition(
        position != std::string::npos,
        "Expected substring should be present before replacement."
    );

    contents.replace(
        position,
        from.size(),
        to
    );

    return contents;
}

std::string removeLineContaining(
    const std::string& contents,
    const std::string& needle
) {
    std::istringstream input(contents);
    std::ostringstream output;
    std::string line;
    bool removed = false;

    while (std::getline(input, line)) {
        if (!removed &&
            line.find(needle) != std::string::npos) {
            removed = true;
            continue;
        }

        output << line << "\n";
    }

    requireCondition(
        removed,
        "Expected line should be present before removal."
    );

    return output.str();
}

void expectDecodeRejected(
    const std::string& contents,
    const std::string& message
) {
    bool rejected = false;

    try {
        (void)FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(contents);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        message
    );
}

KeyPair localValidatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "finalized-block-store-validator"
    );
}

KeyPair localUserKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "finalized-block-store-user"
    );
}

BootstrapValidatorConfig validator(
    const std::string& metadata
) {
    return BootstrapValidatorConfig(
        localValidatorKeyPair().publicKey(),
        1,
        1,
        metadata
    );
}

GenesisConfig genesisConfig() {
    const BootstrapValidatorConfig bootstrap =
        validator("finalized-block-store-validator");

    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            bootstrap
        },
        {
            GenesisAccountConfig(
                localUserKeyPair().address().value(),
                Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "finalized-block-store-genesis"
    );
}

Signer localValidatorSigner() {
    static const Bls12381SignatureProvider provider;

    return Signer(
        localValidatorKeyPair(),
        provider
    );
}

Signer localUserSigner() {
    static const Ed25519SignatureProvider provider;

    return Signer(
        localUserKeyPair(),
        provider
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "finalized-block-store-peer",
        "127.0.0.1:9400",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

Transaction signedTransfer() {
    return nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "store-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            1,
            kTimestamp + 10
        ),
        localUserSigner(),
        "nodo-localnet-1"
    );
}

NodeRuntime startRuntime() {
    const auto result =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                localPeer(),
                16
            )
        );

    requireCondition(
        result.started(),
        "Runtime should start."
    );

    return result.runtime();
}

void admitTransaction(
    NodeRuntime& runtime
) {
    requireCondition(
        runtime.mutableMempool().admitTransaction(
            signedTransfer(),
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "Transaction should be admitted."
    );
}

void testPersistsFinalizedBlockAndUpdatesManifest() {
    const std::filesystem::path path =
        tempPath("persist");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 1
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto pipeline =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 20
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize block."
    );

    requireCondition(
        pipeline.totalFee().rawUnits() == 100,
        "Pipeline should report total transaction fees for the finalized block."
    );

    requireCondition(
        pipeline.feeEconomicBalance().active() &&
        pipeline.feeEconomicBalance().totalFee().rawUnits() == 100 &&
        pipeline.feeEconomicBalance().validatorRewardAmount().rawUnits() == 50 &&
        pipeline.feeEconomicBalance().treasuryAmount().rawUnits() == 30 &&
        pipeline.feeEconomicBalance().burnAmount().rawUnits() == 20,
        "Pipeline should split total block fees into validator reward, treasury and burn."
    );

    requireCondition(
        pipeline.rewardDistributions().size() == 1U &&
        pipeline.rewardDistributions().front().totalReward().rawUnits() == 50 &&
        pipeline.rewardDistributions().front().liquidReward().rawUnits() == 45 &&
        pipeline.rewardDistributions().front().lockedReward().rawUnits() == 5,
        "Pipeline should create validator reward distribution from the validator fee allocation."
    );

    requireCondition(
        pipeline.lockedStakePositions().size() == 1U &&
        pipeline.lockedStakePositions().front().amount().rawUnits() == 5 &&
        pipeline.lockedStakePositions().front().createdAtHeight() == 1U &&
        pipeline.lockedStakePositions().front().unlockAtHeight() > 1U &&
        pipeline.lockedStakePositions().front().slashable(),
        "Pipeline should convert locked rewards into locked stake positions."
    );

    requireCondition(
        pipeline.securityScoreRecords().size() == 1U &&
        pipeline.securityScoreRecords().front().score() == 4 &&
        pipeline.securityScoreRecords().front().lockedStakeScore() == 1 &&
        pipeline.securityScoreRecords().front().participationScore() == 2,
        "Pipeline should calculate security score records from locked stake positions."
    );

    requireCondition(
        pipeline.securityCheckpoints().size() == 1U &&
        pipeline.securityCheckpoints().front().score() == 4 &&
        pipeline.securityCheckpoints().front().band() == "BUILDING" &&
        pipeline.securityCheckpoints().front().lockedStake().rawUnits() == 5,
        "Pipeline should consolidate security score records into validator checkpoints."
    );

    requireCondition(
        pipeline.validatorRiskAssessments().size() == 1U &&
        pipeline.validatorRiskAssessments().front().riskScore() == 996 &&
        pipeline.validatorRiskAssessments().front().riskLevel() == "HIGH" &&
        pipeline.validatorRiskAssessments().front().recommendedAction() == "QUARANTINE_REVIEW",
        "Pipeline should derive validator risk assessments from security checkpoints."
    );

    requireCondition(
        pipeline.validatorContainmentDecisions().size() == 1U &&
        pipeline.validatorContainmentDecisions().front().containmentMode() == "REVIEW_QUARANTINE" &&
        pipeline.validatorContainmentDecisions().front().peerTrustState() == "QUARANTINE_CANDIDATE" &&
        pipeline.validatorContainmentDecisions().front().networkAdmissionState() == "REQUIRE_REVIEW",
        "Pipeline should derive containment decisions from validator risk assessments."
    );

    requireCondition(
        pipeline.validatorNetworkPolicies().size() == 1U &&
        pipeline.validatorNetworkPolicies().front().connectionPolicy() == "MANUAL_REVIEW_ONLY" &&
        pipeline.validatorNetworkPolicies().front().messagePolicy() == "BLOCK_UNTIL_REVIEW" &&
        pipeline.validatorNetworkPolicies().front().consensusPolicy() == "HOLD_FOR_REVIEW" &&
        pipeline.validatorNetworkPolicies().front().requiresManualReview(),
        "Pipeline should derive network policies from containment decisions."
    );

    requireCondition(
        pipeline.monetaryFirewallAudit().passed() &&
        pipeline.monetaryFirewallAudit().supplyLedger().minted().rawUnits() == 0 &&
        pipeline.monetaryFirewallAudit().supplyLedger().burned().rawUnits() == 20 &&
        pipeline.feeBurnRecord().active() &&
        pipeline.treasuryFeeRecord().active(),
        "Pipeline should pass the monetary firewall audit with partial fee burn."
    );

    requireCondition(
        pipeline.genesisTreasurySnapshot().active() &&
        pipeline.protectionRewardBudget().active(),
        "Pipeline should build an active genesis treasury and protection reward budget."
    );

    requireCondition(
        pipeline.protectionRewardSummary().active() &&
        pipeline.protectionWorkRecords().size() == pipeline.protectionRewardGrants().size() &&
        pipeline.protectionRewardSettlements().size() == pipeline.protectionRewardGrants().size(),
        "Pipeline should build real protection reward records."
    );

    requireCondition(
        pipeline.inflationEpochSnapshot().active() &&
        pipeline.mintAuthorizationRecord().isValid() &&
        pipeline.supplyExpansionRecord().isValid() &&
        pipeline.supplyExpansionRecord().mintedAmount().rawUnits() == 0,
        "Pipeline should build controlled issuance records without executing mint."
    );

    requireCondition(
        pipeline.slashingEvidenceSummary().active() &&
        pipeline.slashingEvidenceRecords().size() == pipeline.slashingPreparationRecords().size() &&
        pipeline.slashingEvidenceSummary().evidenceCount() == pipeline.slashingEvidenceRecords().size(),
        "Pipeline should build slashing evidence review records."
    );

    requireCondition(
        pipeline.cryptographicSlashingSummary().active() &&
        pipeline.cryptographicSlashingEvidenceRecords().empty() &&
        pipeline.stakePenaltyRecords().empty(),
        "Pipeline should build an active cryptographic slashing summary without inventing penalties."
    );

    const auto persisted =
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 30
        );

    requireCondition(
        persisted.stored(),
        "Finalized block should be stored."
    );

    requireCondition(
        !std::filesystem::exists(
            FinalizedBlockStore::commitJournalPath(directoryConfig)
        ),
        "Successful finalization must clear its durable commit journal."
    );

    requireCondition(
        std::filesystem::exists(persisted.blockPath()),
        "Finalized block file should exist."
    );

    requireCondition(
        persisted.manifest().latestBlockHeight() == 1U,
        "Manifest should update latest height."
    );

    requireCondition(
        persisted.manifest().latestStateRoot() == pipeline.postStateRoot(),
        "Manifest should update latest state root from finalized block."
    );

    std::ifstream blockFile(persisted.blockPath());
    const std::string blockContents(
        (std::istreambuf_iterator<char>(blockFile)),
        std::istreambuf_iterator<char>()
    );

    requireCondition(
        blockContents.rfind(FinalizedArtifactSchema::currentSchemaId() + "\n", 0) == 0,
        "Finalized block file should use the versionless finalized artifact schema id."
    );

    requireCondition(
        !FinalizedArtifactSchema::hasVersionSuffix(FinalizedArtifactSchema::currentSchemaId()),
        "Current finalized artifact schema id should not have a version suffix."
    );

    requireCondition(
        blockContents.find("postStateRoot=" + pipeline.postStateRoot()) != std::string::npos,
        "Finalized block file should persist post-state root."
    );

    requireCondition(
        blockContents.find("totalFeeRawUnits=100") != std::string::npos,
        "Finalized block file should persist total block fees."
    );

    requireCondition(
        blockContents.find("rewardDistributionCount=1") != std::string::npos &&
        blockContents.find("reward.0.totalRewardRawUnits=50") != std::string::npos &&
        blockContents.find("reward.0.liquidRewardRawUnits=45") != std::string::npos &&
        blockContents.find("reward.0.lockedRewardRawUnits=5") != std::string::npos,
        "Finalized block file should persist validator reward distribution."
    );

    requireCondition(
        blockContents.find("lockedStakePositionCount=1") != std::string::npos &&
        blockContents.find("lockedStake.0.amountRawUnits=5") != std::string::npos &&
        blockContents.find("lockedStake.0.createdAtHeight=1") != std::string::npos &&
        blockContents.find("lockedStake.0.slashable=true") != std::string::npos,
        "Finalized block file should persist locked stake position."
    );

    requireCondition(
        blockContents.find("securityScoreRecordCount=1") != std::string::npos &&
        blockContents.find("securityScore.0.score=4") != std::string::npos &&
        blockContents.find("securityScore.0.lockedStakeScore=1") != std::string::npos &&
        blockContents.find("securityScore.0.participationScore=2") != std::string::npos &&
        blockContents.find("securityScore.0.reason=LOCKED_STAKE_REWARD") != std::string::npos,
        "Finalized block file should persist security score records."
    );

    requireCondition(
        blockContents.find("securityCheckpointCount=1") != std::string::npos &&
        blockContents.find("securityCheckpoint.0.score=4") != std::string::npos &&
        blockContents.find("securityCheckpoint.0.band=BUILDING") != std::string::npos &&
        blockContents.find("securityCheckpoint.0.lockedStakeRawUnits=5") != std::string::npos &&
        blockContents.find("securityCheckpoint.0.reason=SECURITY_SCORE_CHECKPOINT") != std::string::npos,
        "Finalized block file should persist validator security checkpoints."
    );

    requireCondition(
        blockContents.find("validatorRiskAssessmentCount=1") != std::string::npos &&
        blockContents.find("validatorRisk.0.riskScore=996") != std::string::npos &&
        blockContents.find("validatorRisk.0.riskLevel=HIGH") != std::string::npos &&
        blockContents.find("validatorRisk.0.recommendedAction=QUARANTINE_REVIEW") != std::string::npos &&
        blockContents.find("validatorRisk.0.reason=VALIDATOR_SECURITY_RISK_ASSESSMENT") != std::string::npos,
        "Finalized block file should persist validator risk assessments."
    );

    requireCondition(
        blockContents.find("validatorContainmentDecisionCount=1") != std::string::npos &&
        blockContents.find("validatorContainment.0.containmentMode=REVIEW_QUARANTINE") != std::string::npos &&
        blockContents.find("validatorContainment.0.peerTrustState=QUARANTINE_CANDIDATE") != std::string::npos &&
        blockContents.find("validatorContainment.0.networkAdmissionState=REQUIRE_REVIEW") != std::string::npos &&
        blockContents.find("validatorContainment.0.reason=VALIDATOR_CONTAINMENT_DECISION") != std::string::npos,
        "Finalized block file should persist validator containment decisions."
    );

    requireCondition(
        blockContents.find("validatorNetworkPolicyCount=1") != std::string::npos &&
        blockContents.find("validatorNetworkPolicy.0.connectionPolicy=MANUAL_REVIEW_ONLY") != std::string::npos &&
        blockContents.find("validatorNetworkPolicy.0.messagePolicy=BLOCK_UNTIL_REVIEW") != std::string::npos &&
        blockContents.find("validatorNetworkPolicy.0.consensusPolicy=HOLD_FOR_REVIEW") != std::string::npos &&
        blockContents.find("validatorNetworkPolicy.0.requiresManualReview=true") != std::string::npos &&
        blockContents.find("validatorNetworkPolicy.0.reason=VALIDATOR_NETWORK_POLICY") != std::string::npos,
        "Finalized block file should persist validator network policies."
    );

    requireCondition(
        blockContents.find("monetaryFirewallStatus=PASS") != std::string::npos &&
        blockContents.find("monetary.mintedRawUnits=0") != std::string::npos &&
        blockContents.find("monetary.burnedRawUnits=20") != std::string::npos &&
        blockContents.find("monetary.treasuryDeltaRawUnits=30") != std::string::npos &&
        blockContents.find("monetary.reason=MONETARY_FIREWALL_ZERO_MINT") != std::string::npos,
        "Finalized block file should persist monetary firewall audit with fee burn."
    );

    requireCondition(
        blockContents.find("supplyDelta.blockHeight=1") != std::string::npos &&
        blockContents.find("supplyDelta.blockHash=" + pipeline.block().hash()) != std::string::npos &&
        blockContents.find("supplyDelta.supplyBeforeRawUnits=1000000000000") != std::string::npos &&
        blockContents.find("supplyDelta.mintedAmountRawUnits=0") != std::string::npos &&
        blockContents.find("supplyDelta.burnedAmountRawUnits=20") != std::string::npos &&
        blockContents.find("supplyDelta.supplyAfterRawUnits=999999999980") != std::string::npos &&
        blockContents.find("supplyDelta.mintRecordCount=0") != std::string::npos &&
        blockContents.find("supplyDelta.burnRecordCount=1") != std::string::npos &&
        blockContents.find("supplyDelta.burn.0.amountRawUnits=20") != std::string::npos &&
        blockContents.find("supplyDelta.burn.0.burnType=FEE_BURN") != std::string::npos,
        "Finalized block file should persist canonical SupplyDelta fields."
    );

    const auto decodedArtifact =
        FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(blockContents);

    requireCondition(
        decodedArtifact.supplyDelta().blockHeight() == pipeline.supplyDelta().blockHeight() &&
        decodedArtifact.supplyDelta().blockHash() == pipeline.supplyDelta().blockHash() &&
        decodedArtifact.supplyDelta().supplyBefore() == pipeline.supplyDelta().supplyBefore() &&
        decodedArtifact.supplyDelta().burnedAmount() == pipeline.supplyDelta().burnedAmount() &&
        decodedArtifact.supplyDelta().supplyAfter() == pipeline.supplyDelta().supplyAfter(),
        "Finalized block codec should round-trip the persisted SupplyDelta."
    );

    expectDecodeRejected(
        removeLineContaining(blockContents, "supplyDelta.supplyAfterRawUnits="),
        "Finalized block codec should reject artifacts missing SupplyDelta fields."
    );

    expectDecodeRejected(
        replaceFirst(
            blockContents,
            "supplyDelta.blockHeight=1",
            "supplyDelta.blockHeight=2"
        ),
        "Finalized block codec should reject a SupplyDelta block-height mismatch."
    );

    expectDecodeRejected(
        replaceFirst(
            blockContents,
            "supplyDelta.blockHash=" + pipeline.block().hash(),
            "supplyDelta.blockHash=tampered-supply-delta-hash"
        ),
        "Finalized block codec should reject a SupplyDelta block-hash mismatch."
    );

    expectDecodeRejected(
        blockContents + "unexpectedSupplyDeltaField=1\n",
        "Finalized block codec should reject unexpected artifact fields."
    );

    expectDecodeRejected(
        replaceFirst(
            blockContents,
            FinalizedArtifactSchema::currentSchemaId(),
            "NODO_FINALIZED_BLOCK_V19"
        ),
        "Finalized block codec should reject the legacy V19 schema id."
    );

    expectDecodeRejected(
        replaceFirst(
            blockContents,
            FinalizedArtifactSchema::currentSchemaId(),
            "NODO_FINALIZED_BLOCK_V20"
        ),
        "Finalized block codec should reject the legacy V20 schema id."
    );

    requireCondition(
        blockContents.find("feeEconomicBalanceStatus=ACTIVE") != std::string::npos &&
        blockContents.find("feeBalance.totalFeeRawUnits=100") != std::string::npos &&
        blockContents.find("feeBalance.validatorRewardRawUnits=50") != std::string::npos &&
        blockContents.find("feeBalance.treasuryRawUnits=30") != std::string::npos &&
        blockContents.find("feeBalance.burnRawUnits=20") != std::string::npos &&
        blockContents.find("feeBurnStatus=ACTIVE") != std::string::npos &&
        blockContents.find("feeBurn.burnAmountRawUnits=20") != std::string::npos &&
        blockContents.find("treasuryFeeStatus=ACTIVE") != std::string::npos &&
        blockContents.find("treasuryFee.treasuryAmountRawUnits=30") != std::string::npos,
        "Finalized block file should persist fee economics records."
    );

    requireCondition(
        blockContents.find("genesisTreasuryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("treasury.treasuryAddress=treasury-protocol-account") != std::string::npos &&
        blockContents.find("protectionRewardBudgetStatus=ACTIVE") != std::string::npos &&
        blockContents.find("protectionBudget.reason=INITIAL_PROTECTION_REWARD_BUDGET") != std::string::npos,
        "Finalized block file should persist genesis treasury and protection reward budget."
    );

    requireCondition(
        blockContents.find("protectionWorkRecordCount=1") != std::string::npos &&
        blockContents.find("protectionRewardSummaryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("protectionRewardSettlementCount=1") != std::string::npos &&
        blockContents.find("protectionSummary.reason=REAL_PROTECTION_REWARD_SUMMARY") != std::string::npos &&
        blockContents.find("protectionSettlement.0.reason=PROTECTION_REWARD_SETTLEMENT") != std::string::npos,
        "Finalized block file should persist real protection reward records."
    );

    requireCondition(
        blockContents.find("inflationEpochStatus=ACTIVE") != std::string::npos &&
        blockContents.find("inflationEpoch.maxAnnualInflationBasisPoints=400") != std::string::npos &&
        blockContents.find("mintAuthorizationStatus=NONE") != std::string::npos &&
        blockContents.find("mintAuthorization.reason=NO_ACTIVE_MINT_AUTHORIZATION") != std::string::npos &&
        blockContents.find("supplyExpansionStatus=NONE") != std::string::npos &&
        blockContents.find("supplyExpansion.mintedAmountRawUnits=0") != std::string::npos,
        "Finalized block file should persist controlled issuance records."
    );

    requireCondition(
        blockContents.find("slashingEvidenceSummaryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("slashingEvidenceRecordCount=1") != std::string::npos &&
        blockContents.find("slashingPreparationRecordCount=1") != std::string::npos &&
        blockContents.find("slashingEvidence.0.reason=RISK_CONTAINMENT_EVIDENCE") != std::string::npos &&
        blockContents.find("slashingPreparation.0.reason=SLASHING_PREPARATION_REVIEW") != std::string::npos &&
        blockContents.find("slashingSummary.reason=SLASHING_EVIDENCE_SUMMARY") != std::string::npos,
        "Finalized block file should persist slashing evidence preparation records."
    );

    requireCondition(
        blockContents.find("cryptographicSlashingSummaryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("cryptographicSlashingEvidenceCount=0") != std::string::npos &&
        blockContents.find("stakePenaltyRecordCount=0") != std::string::npos &&
        blockContents.find("cryptographicSlashingSummary.reason=CRYPTOGRAPHIC_SLASHING_SUMMARY") != std::string::npos,
        "Finalized block file should persist cryptographic slashing accounting records."
    );

    requireCondition(
        blockContents.find("governancePolicyStatus=ACTIVE") != std::string::npos &&
        blockContents.find("governanceActionGuardCount=2") != std::string::npos &&
        blockContents.find("governanceGuard.0.actionType=TREASURY_SPEND") != std::string::npos &&
        blockContents.find("governanceGuard.1.actionType=MINT_AUTHORIZATION") != std::string::npos &&
        blockContents.find("governanceSummaryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("governanceSummary.reason=GOVERNANCE_NO_ACTIVE_PROPOSALS") != std::string::npos,
        "Finalized block file should persist governance guards for treasury and controlled issuance."
    );

    requireCondition(
        blockContents.find("validatorLifecycleRecordCount=1") != std::string::npos &&
        blockContents.find("validatorLifecycle.0.lifecycleStatus=JAILED") != std::string::npos &&
        blockContents.find("epochAccountingStatus=ACTIVE") != std::string::npos &&
        blockContents.find("epochAccounting.activeValidatorCount=0") != std::string::npos &&
        blockContents.find("epochAccounting.epochIndex=1") != std::string::npos &&
        blockContents.find("validatorLifecycleSummaryStatus=ACTIVE") != std::string::npos &&
        blockContents.find("validatorLifecycleSummary.activeValidatorCount=0") != std::string::npos &&
        blockContents.find("validatorLifecycleSummary.jailedValidatorCount=1") != std::string::npos &&
        blockContents.find("validatorLifecycleSummary.reason=VALIDATOR_LIFECYCLE_SUMMARY") != std::string::npos,
        "Finalized block file should persist validator lifecycle and epoch accounting."
    );

    const auto loaded =
        NodeDataDirectory::loadManifest(directoryConfig);

    requireCondition(
        loaded.loaded() &&
        loaded.manifest().latestBlockHeight() == 1U &&
        loaded.manifest().latestStateRoot() == pipeline.postStateRoot(),
        "Updated manifest should reload with latest state root."
    );

    clean(path);
}

void testPersistIsIdempotentForSameFinalizedBlock() {
    const std::filesystem::path path =
        tempPath("idempotent");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 40
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto pipeline =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 50
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize block."
    );

    requireCondition(
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 60
        ).stored(),
        "First persist should store block."
    );

    requireCondition(
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 61
        ).alreadyStored(),
        "Second persist should be idempotent."
    );

    clean(path);
}

void testRejectsPersistBeforeInit() {
    const std::filesystem::path path =
        tempPath("missing");

    clean(path);

    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto pipeline =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 70
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize block."
    );

    requireCondition(
        !FinalizedBlockStore::persist(
            NodeDataDirectoryConfig(path),
            runtime,
            pipeline,
            kTimestamp + 80
        ).success(),
        "Persist before init should fail safely."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testPersistsFinalizedBlockAndUpdatesManifest();
        testPersistIsIdempotentForSameFinalizedBlock();
        testRejectsPersistBeforeInit();

        std::cout << "Nodo finalized block store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo finalized block store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
