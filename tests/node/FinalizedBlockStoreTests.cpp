#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
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

PublicKey publicKey(
    const std::string& suffix
) {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "finalized-block-store-public-key-" + suffix
    );
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "finalized-block-store-private-key-" + suffix
    );
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        publicKey("validator-" + suffix),
        1,
        1,
        "finalized-block-store-validator-" + suffix
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator("a"),
            validator("b")
        },
        "finalized-block-store-genesis"
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
    Transaction transaction(
        TransactionType::TRANSFER,
        "store-sender",
        "store-recipient",
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(100),
        1,
        kTimestamp + 10
    );

    transaction.attachSignatureBundle(
        SignatureBundle::createDevelopmentSignature(
            transaction.signingPayload(),
            publicKey("tx"),
            privateKey("tx"),
            kTimestamp + 10
        )
    );

    return transaction;
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
            )
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize block."
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

    const auto loaded =
        NodeDataDirectory::loadManifest(directoryConfig);

    requireCondition(
        loaded.loaded() &&
        loaded.manifest().latestBlockHeight() == 1U,
        "Updated manifest should reload."
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
            )
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
            )
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
