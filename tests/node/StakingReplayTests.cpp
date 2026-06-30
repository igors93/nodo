#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionBuildRequest;
using nodo::core::TransactionBuilder;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::node::FinalizedBlockStore;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::node::RuntimeStateLoader;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr std::int64_t kStake     = 1'000'000;
constexpr std::int64_t kFee       = 100;
constexpr std::int64_t kGenesisStake = 1'000'000;
const std::string kChainId        = "nodo-localnet-1";

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / ("nodo-staking-replay-tests-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair("staking-replay-validator");
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("staking-replay-user");
}

Signer validatorSigner() {
    static const Bls12381SignatureProvider provider;
    return Signer(validatorKeyPair(), provider);
}

Signer userSigner() {
    static const Ed25519SignatureProvider provider;
    return Signer(userKeyPair(), provider);
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            BootstrapValidatorConfig(
                validatorKeyPair().publicKey(),
                1, 1,
                "staking-replay-validator"
            )
        },
        {
            GenesisAccountConfig(
                userKeyPair().address().value(),
                Amount::fromRawUnits(1'000'000'000'000LL),
                0
            )
        },
        "staking-replay-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "staking-replay-peer",
        "127.0.0.1:9902",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

NodeRuntime startRuntime() {
    const auto result = NodeRuntimeFactory::startFromGenesis(
        NodeRuntimeConfig(genesisConfig(), localPeer(), 16)
    );
    requireCondition(result.started(), "Runtime should start from genesis.");
    return result.runtime();
}

// Improvement 2: loadFromDataDirectory must replay staking transactions from each
// persisted block so the rebuilt runtime's StakingRegistry matches the original.
void testStakingRegistryRebuiltAfterReload() {
    const std::filesystem::path path = tempPath("deposit");
    clean(path);

    const NodeDataDirectoryConfig dirConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            dirConfig, genesisConfig(), localPeer(), kTimestamp + 1
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    const Transaction tx = TransactionBuilder::buildSignedStakeDeposit(
        TransactionBuildRequest(
            validatorAddr,
            Amount::fromRawUnits(kStake),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            tx, CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION, kTimestamp + 11
        ).accepted(),
        "STAKE_DEPOSIT should be admitted to mempool."
    );

    const auto pipeline = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    requireCondition(pipeline.finalized(), "Block with STAKE_DEPOSIT should finalize.");

    const auto persistResult =
        FinalizedBlockStore::persist(dirConfig, runtime, pipeline, kTimestamp + 30);
    requireCondition(
        persistResult.stored(),
        "Finalized block should persist to disk. Status="
            + nodo::node::finalizedBlockStoreStatusToString(persistResult.status())
            + " Reason=" + persistResult.reason()
    );

    const std::int64_t bondedBefore =
        runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits();

    requireCondition(
        bondedBefore == kGenesisStake + kStake,
        "Original runtime bonded amount should equal genesis plus stake. Got: "
            + std::to_string(bondedBefore)
    );

    const auto loaded = RuntimeStateLoader::loadFromDataDirectory(
        dirConfig, genesisConfig(), localPeer()
    );

    requireCondition(
        loaded.loaded(),
        "Runtime should reload from data directory. Status=" +
            nodo::node::runtimeStateLoadStatusToString(loaded.status()) +
            " Reason=" + loaded.reason()
    );

    requireCondition(
        loaded.loadedBlockCount() == 1U,
        "Loader should report one loaded block."
    );

    const std::int64_t bondedAfter =
        loaded.runtime()
            .stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits();

    requireCondition(
        bondedAfter == kGenesisStake + kStake,
        "Reloaded runtime StakingRegistry must match original bonded amount. Got: "
            + std::to_string(bondedAfter)
    );

    clean(path);
}

// Improvement 2 (continued): two staking blocks persisted — STAKE_DEPOSIT then
// STAKE_TOP_UP — both are replayed to produce the correct accumulated total.
void testMultipleStakingBlocksReplayedCorrectly() {
    const std::filesystem::path path = tempPath("multi");
    clean(path);

    const NodeDataDirectoryConfig dirConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            dirConfig, genesisConfig(), localPeer(), kTimestamp + 1
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    // Block 1: deposit
    {
        const Transaction tx = TransactionBuilder::buildSignedStakeDeposit(
            TransactionBuildRequest(
                validatorAddr,
                Amount::fromRawUnits(kStake),
                Amount::fromRawUnits(kFee),
                1,
                kTimestamp + 10
            ),
            userSigner(), kChainId
        );
        requireCondition(
            runtime.mutableMempool().admitTransaction(
                tx, CryptoPolicy::developmentPolicy(),
                SecurityContext::USER_TRANSACTION, kTimestamp + 11
            ).accepted(),
            "Block 1: STAKE_DEPOSIT should be admitted."
        );
        const auto p1 = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
            runtime, RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
            validatorSigner()
        );
        requireCondition(p1.finalized(), "Block 1 should finalize.");
        requireCondition(
            FinalizedBlockStore::persist(dirConfig, runtime, p1, kTimestamp + 25).stored(),
            "Block 1 should persist."
        );
    }

    // Block 2: top-up
    {
        const Transaction tx = TransactionBuilder::buildSignedStakeTopUp(
            TransactionBuildRequest(
                validatorAddr,
                Amount::fromRawUnits(kStake),
                Amount::fromRawUnits(kFee),
                2,
                kTimestamp + 30
            ),
            userSigner(), kChainId
        );
        requireCondition(
            runtime.mutableMempool().admitTransaction(
                tx, CryptoPolicy::developmentPolicy(),
                SecurityContext::USER_TRANSACTION, kTimestamp + 31
            ).accepted(),
            "Block 2: STAKE_TOP_UP should be admitted."
        );
        const auto p2 = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
            runtime, RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 40),
            validatorSigner()
        );
        requireCondition(p2.finalized(), "Block 2 should finalize.");
        requireCondition(
            FinalizedBlockStore::persist(dirConfig, runtime, p2, kTimestamp + 45).stored(),
            "Block 2 should persist."
        );
    }

    const std::int64_t expectedBonded = kGenesisStake + (kStake * 2);

    const auto loaded = RuntimeStateLoader::loadFromDataDirectory(
        dirConfig, genesisConfig(), localPeer()
    );
    requireCondition(
        loaded.loaded(),
        "Runtime should reload from data directory. Reason=" + loaded.reason()
    );
    requireCondition(
        loaded.loadedBlockCount() == 2U,
        "Loader should report two loaded blocks."
    );

    const std::int64_t bondedAfter =
        loaded.runtime()
            .stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits();

    requireCondition(
        bondedAfter == expectedBonded,
        "Reloaded StakingRegistry must accumulate both blocks. Got: "
            + std::to_string(bondedAfter)
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testStakingRegistryRebuiltAfterReload();
        testMultipleStakingBlocksReplayedCorrectly();
        std::cout << "Staking replay tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Staking replay tests failed: " << error.what() << "\n";
        return 1;
    }
}
