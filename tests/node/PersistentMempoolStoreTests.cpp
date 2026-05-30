#include "node/PersistentMempoolStore.hpp"

#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/NodeDataDirectory.hpp"
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
using nodo::mempool::Mempool;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::PersistentMempoolStore;
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
        / ("nodo-persistent-mempool-tests-" + suffix);
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
        "persistent-mempool-public-key-" + suffix
    );
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "persistent-mempool-private-key-" + suffix
    );
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        publicKey("validator-" + suffix),
        1,
        1,
        "persistent-mempool-validator-" + suffix
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
        "persistent-mempool-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "persistent-mempool-peer",
        "127.0.0.1:9700",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

Transaction signedTransfer(
    const std::string& suffix,
    std::uint64_t nonce
) {
    Transaction transaction(
        TransactionType::TRANSFER,
        "persistent-mempool-sender",
        "persistent-mempool-recipient",
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(100),
        nonce,
        kTimestamp + static_cast<std::int64_t>(nonce)
    );

    transaction.attachSignatureBundle(
        SignatureBundle::createDevelopmentSignature(
            transaction.signingPayload(),
            publicKey("tx-" + suffix),
            privateKey("tx-" + suffix),
            transaction.timestamp()
        )
    );

    return transaction;
}

void initDirectory(
    const NodeDataDirectoryConfig& directoryConfig
) {
    requireCondition(
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 1
        ).initialized(),
        "Data directory should initialize."
    );
}

void testPersistLoadAndRemoveTransaction() {
    const std::filesystem::path path =
        tempPath("store-load-remove");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);
    initDirectory(directoryConfig);

    const Transaction transaction =
        signedTransfer("a", 1);

    const auto persisted =
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            publicKey("tx-a"),
            kTimestamp + 20
        );

    requireCondition(
        persisted.stored(),
        "Persistent transaction should store."
    );

    Mempool mempool;

    const auto loaded =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        );

    requireCondition(
        loaded.loaded() &&
        loaded.loadedTransactionCount() == 1U &&
        mempool.size() == 1U,
        "Persistent transaction should load into mempool."
    );

    requireCondition(
        PersistentMempoolStore::removeTransactions(
            directoryConfig,
            {transaction.id()}
        ) == 1U,
        "Persistent transaction should be removed."
    );

    Mempool emptyMempool;

    requireCondition(
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            emptyMempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ).loadedTransactionCount() == 0U,
        "Removed persistent transaction should not reload."
    );

    clean(path);
}

void testPersistIsIdempotent() {
    const std::filesystem::path path =
        tempPath("idempotent");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);
    initDirectory(directoryConfig);

    const Transaction transaction =
        signedTransfer("b", 2);

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            publicKey("tx-b"),
            kTimestamp + 30
        ).stored(),
        "First persistent transaction write should store."
    );

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            publicKey("tx-b"),
            kTimestamp + 30
        ).alreadyStored(),
        "Second identical persistent transaction write should be idempotent."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testPersistLoadAndRemoveTransaction();
        testPersistIsIdempotent();

        std::cout << "Nodo persistent mempool store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo persistent mempool store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
