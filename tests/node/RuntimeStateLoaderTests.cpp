#include "node/RuntimeStateLoader.hpp"

#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
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
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
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

std::string readFile(
    const std::filesystem::path& path
) {
    std::ifstream input(path);
    return std::string(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
}

void writeFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream output(path, std::ios::trunc);
    output << contents;
}

std::string replaceAll(
    std::string value,
    const std::string& needle,
    const std::string& replacement
) {
    std::size_t position = 0;

    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(
            position,
            needle.size(),
            replacement
        );
        position += replacement.size();
    }

    return value;
}

KeyPair localValidatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "runtime-state-loader-validator"
    );
}

KeyPair localUserKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "runtime-state-loader-user"
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
        validator("runtime-state-loader-validator");

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
        "runtime-state-loader-genesis"
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
        localUserSigner()
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
            1,
            kTimestamp + 50
        );

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            localUserKeyPair().publicKey(),
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

void testRejectsFinalizedBlockWithTamperedPostStateRoot() {
    const std::filesystem::path path =
        tempPath("tampered-state-root");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 60
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime =
        startRuntime();

    const Transaction transaction =
        signedTransfer(
            "tamper",
            1,
            kTimestamp + 70
        );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 71
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
                kTimestamp + 80
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize a block."
    );

    const auto persisted =
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 90
        );

    requireCondition(
        persisted.stored(),
        "Finalized block should persist before tampering."
    );

    std::string contents =
        readFile(persisted.blockPath());

    const std::string expected =
        "postStateRoot=" + pipeline.postStateRoot();

    const std::size_t position =
        contents.find(expected);

    requireCondition(
        position != std::string::npos,
        "Persisted block should contain expected postStateRoot."
    );

    contents.replace(
        position,
        expected.size(),
        "postStateRoot=bad-state-root"
    );

    writeFile(
        persisted.blockPath(),
        contents
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.status() == nodo::node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID &&
        loaded.reason().find("postStateRoot") != std::string::npos,
        "Runtime loader should reject finalized block with tampered postStateRoot."
    );

    clean(path);
}

void testRejectsManifestWithTamperedLatestStateRoot() {
    const std::filesystem::path path =
        tempPath("tampered-manifest-state-root");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    const auto initialized =
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 100
        );

    requireCondition(
        initialized.initialized(),
        "Data directory should initialize."
    );

    std::string manifestContents =
        readFile(directoryConfig.manifestPath());

    manifestContents = replaceAll(
        manifestContents,
        "latestStateRoot=" + initialized.manifest().latestStateRoot(),
        "latestStateRoot=tampered-state-root"
    );

    writeFile(
        directoryConfig.manifestPath(),
        manifestContents
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.status() == nodo::node::RuntimeStateLoadStatus::MANIFEST_MISMATCH &&
        loaded.reason().find("latestStateRoot") != std::string::npos,
        "Runtime loader should reject manifest with tampered latestStateRoot."
    );

    clean(path);
}

void testRejectsFinalizedBlockWithInvalidQuorumCertificate() {
    const std::filesystem::path path =
        tempPath("invalid-quorum");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 110
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime =
        startRuntime();

    const Transaction transaction =
        signedTransfer(
            "invalid-quorum",
            1,
            kTimestamp + 111
        );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 112
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
                kTimestamp + 113
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize a block."
    );

    const auto persisted =
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 114
        );

    requireCondition(
        persisted.stored(),
        "Finalized block should persist before quorum tampering."
    );

    std::string contents =
        readFile(persisted.blockPath());

    contents = replaceAll(
        contents,
        "requiredVoteCount=1",
        "requiredVoteCount=2"
    );

    writeFile(
        persisted.blockPath(),
        contents
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.status() == nodo::node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
        "Runtime loader should reject finalized block with invalid quorum certificate."
    );

    clean(path);
}

void testRejectsFinalizedBlockWithDuplicateVote() {
    const std::filesystem::path path =
        tempPath("duplicate-vote");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 120
        ).initialized(),
        "Data directory should initialize."
    );

    NodeRuntime runtime =
        startRuntime();

    const Transaction transaction =
        signedTransfer(
            "duplicate-vote",
            1,
            kTimestamp + 121
        );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 122
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
                kTimestamp + 123
            ),
            localValidatorSigner()
        );

    requireCondition(
        pipeline.finalized(),
        "Pipeline should finalize a block."
    );

    const auto persisted =
        FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            kTimestamp + 124
        );

    requireCondition(
        persisted.stored(),
        "Finalized block should persist before vote tampering."
    );

    const std::string serializedVote =
        pipeline.certificate().votes().front().serialize();

    std::string contents =
        readFile(persisted.blockPath());

    contents = replaceAll(
        contents,
        "votes=[" + serializedVote + "]",
        "votes=[" + serializedVote + "," + serializedVote + "]"
    );

    writeFile(
        persisted.blockPath(),
        contents
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.status() == nodo::node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
        "Runtime loader should reject finalized block with duplicate validator vote."
    );

    clean(path);
}

void testRejectsPersistentMempoolFutureNonce() {
    const std::filesystem::path path =
        tempPath("mempool-future-nonce");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 130
        ).initialized(),
        "Data directory should initialize."
    );

    const Transaction transaction =
        signedTransfer(
            "future",
            2,
            kTimestamp + 131
        );

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            localUserKeyPair().publicKey(),
            kTimestamp + 132
        ).stored(),
        "Persistent mempool transaction should store before reload nonce audit."
    );

    const auto loaded =
        RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig(),
            localPeer()
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.status() == nodo::node::RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED &&
        loaded.reason().find("future") != std::string::npos,
        "Runtime loader should reject persistent mempool transaction with future nonce."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testLoadsRuntimeWithPersistedFinalizedBlock();
        testLoadsPersistentMempoolIntoRuntime();
        testRejectsFinalizedBlockWithTamperedPostStateRoot();
        testRejectsManifestWithTamperedLatestStateRoot();
        testRejectsFinalizedBlockWithInvalidQuorumCertificate();
        testRejectsFinalizedBlockWithDuplicateVote();
        testRejectsPersistentMempoolFutureNonce();

        std::cout << "Nodo runtime state loader tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo runtime state loader tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
