#include "node/RuntimeStateLoader.hpp"

#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/LocalSignatureProvider.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeBlockPipeline.hpp"
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
using nodo::crypto::KeyPair;
using nodo::crypto::LocalSignatureProvider;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::crypto::SignatureBundle;
using nodo::node::FinalizedBlockStore;
using nodo::node::FinalizedBlockFileCodec;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::PersistentMempoolStore;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::node::RuntimeStateLoader;
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
        / ("nodo-runtime-state-loader-tests-" + suffix);
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
        "runtime-state-loader-validator"
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
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator("runtime-state-loader-validator")
        },
        "runtime-state-loader-genesis"
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
        "runtime-state-loader-peer",
        "127.0.0.1:9600",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

Transaction signedTransfer(
    const std::string& suffix,
    std::uint64_t nonce,
    std::int64_t timestamp
) {
    (void)suffix;

    return nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "runtime-state-loader-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce,
            timestamp
        ),
        localValidatorSigner()
    );
}

NodeRuntime startRuntime() {
    const auto start =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                localPeer(),
                16
            )
        );

    requireCondition(
        start.started(),
        "Runtime should start."
    );

    return start.runtime();
}

void testLoadsRuntimeWithPersistedFinalizedBlock() {
    const std::filesystem::path path =
        tempPath("finalized-block");

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

    const Transaction transaction =
        signedTransfer(
            "a",
            1,
            kTimestamp + 10
        );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "Transaction should enter mempool."
    );

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
        "Pipeline should finalize a block."
    );

    try {
        (void)FinalizedBlockFileCodec::decodeBlockFileContents(
            FinalizedBlockStore::finalizedBlockFileContents(pipeline)
            + "unknownField=must-fail\n"
        );

        throw std::runtime_error("Finalized block codec accepted an unknown field.");
    } catch (const std::invalid_argument& error) {
        requireCondition(
            std::string(error.what()).find("unknownField") != std::string::npos,
            "Finalized block codec should name the unknown field."
        );
    }

    requireCondition(
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 30
        ).stored(),
        "Finalized block should persist."
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        loaded.loaded(),
        "Runtime should load from persisted data directory."
    );

    requireCondition(
        loaded.runtime().blockchain().size() == 2U &&
        loaded.runtime().blockchain().latestBlock().hash() == runtime.blockchain().latestBlock().hash(),
        "Loaded runtime should include persisted finalized block."
    );

    requireCondition(
        loaded.loadedBlockCount() == 1U,
        "Loader should report one loaded non-genesis block."
    );

    clean(path);
}

void testLoadsPersistentMempoolIntoRuntime() {
    const std::filesystem::path path =
        tempPath("mempool");

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

    const Transaction transaction =
        signedTransfer(
            "b",
            2,
            kTimestamp + 50
        );

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            localValidatorKeyPair().publicKey(),
            kTimestamp + 51
        ).stored(),
        "Persistent mempool transaction should store."
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        loaded.loaded(),
        "Runtime should load with persistent mempool."
    );

    requireCondition(
        loaded.loadedMempoolTransactionCount() == 1U &&
        loaded.runtime().mempool().size() == 1U,
        "Persistent mempool should be loaded into runtime."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testLoadsRuntimeWithPersistedFinalizedBlock();
        testLoadsPersistentMempoolIntoRuntime();

        std::cout << "Nodo runtime state loader tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo runtime state loader tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
