#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/LocalSignatureProvider.hpp"
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
using nodo::crypto::LocalSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::crypto::SignatureBundle;
using nodo::node::FinalizedBlockStore;
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

KeyPair localValidatorKeyPair() {
    return KeyPair::createDevelopmentKeyPair(
        "finalized-block-store-validator"
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
                bootstrap.validatorAddress(),
                Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "finalized-block-store-genesis"
    );
}

Signer localValidatorSigner() {
    static const LocalSignatureProvider provider;

    return Signer(
        localValidatorKeyPair(),
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
        localValidatorSigner()
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
        pipeline.rewardDistributions().size() == 1U &&
        pipeline.rewardDistributions().front().totalReward().rawUnits() == 100 &&
        pipeline.rewardDistributions().front().liquidReward().rawUnits() == 90 &&
        pipeline.rewardDistributions().front().lockedReward().rawUnits() == 10,
        "Pipeline should create validator reward distribution from block fees."
    );

    requireCondition(
        pipeline.lockedStakePositions().size() == 1U &&
        pipeline.lockedStakePositions().front().amount().rawUnits() == 10 &&
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
        pipeline.securityCheckpoints().front().lockedStake().rawUnits() == 10,
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
        blockContents.find("postStateRoot=" + pipeline.postStateRoot()) != std::string::npos,
        "Finalized block file should persist post-state root."
    );

    requireCondition(
        blockContents.find("totalFeeRawUnits=100") != std::string::npos,
        "Finalized block file should persist total block fees."
    );

    requireCondition(
        blockContents.find("rewardDistributionCount=1") != std::string::npos &&
        blockContents.find("reward.0.totalRewardRawUnits=100") != std::string::npos &&
        blockContents.find("reward.0.liquidRewardRawUnits=90") != std::string::npos &&
        blockContents.find("reward.0.lockedRewardRawUnits=10") != std::string::npos,
        "Finalized block file should persist validator reward distribution."
    );

    requireCondition(
        blockContents.find("lockedStakePositionCount=1") != std::string::npos &&
        blockContents.find("lockedStake.0.amountRawUnits=10") != std::string::npos &&
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
        blockContents.find("securityCheckpoint.0.lockedStakeRawUnits=10") != std::string::npos &&
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
