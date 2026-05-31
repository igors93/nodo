#include "node/PersistentMempoolStore.hpp"

#include "config/NetworkParameters.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "node/NodeDataDirectory.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::AccountState;
using nodo::core::AccountStateView;
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
using nodo::crypto::SignatureBundle;
using nodo::crypto::SigningDomain;
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

KeyPair validatorKeyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "persistent-mempool-validator-key-" + suffix
    );
}

PublicKey validatorPublicKey(
    const std::string& suffix
) {
    return validatorKeyPair(suffix).publicKey();
}

KeyPair transactionKeyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicEd25519KeyPair(
        "persistent-mempool-transaction-key-" + suffix
    );
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        validatorPublicKey(suffix),
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

    const KeyPair keyPair =
        transactionKeyPair(suffix);
    const Ed25519SignatureProvider provider;

    transaction.attachSignatureBundle(
        SignatureBundle::createSignature(
            transaction.signingPayload(),
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
            transaction.timestamp(),
            provider,
            SigningDomain::USER_TRANSACTION
        )
    );

    return transaction;
}

AccountStateView accountStateWithSenderBalance(
    std::int64_t balanceRawUnits
) {
    AccountStateView view;

    requireCondition(
        view.putAccount(
            AccountState(
                "persistent-mempool-sender",
                Amount::fromRawUnits(balanceRawUnits),
                0
            )
        ),
        "Account state test fixture should accept sender account."
    );

    return view;
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
            transactionKeyPair("a").publicKey(),
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
            transactionKeyPair("b").publicKey(),
            kTimestamp + 30
        ).stored(),
        "First persistent transaction write should store."
    );

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            transactionKeyPair("b").publicKey(),
            kTimestamp + 30
        ).alreadyStored(),
        "Second identical persistent transaction write should be idempotent."
    );

    clean(path);
}

void testLoadWithAccountStateAcceptsSufficientBalance() {
    const std::filesystem::path path =
        tempPath("sufficient-balance");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);
    initDirectory(directoryConfig);

    const Transaction transaction =
        signedTransfer("c", 1);

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            transactionKeyPair("c").publicKey(),
            kTimestamp + 40
        ).stored(),
        "Persistent transaction fixture should store before balance-aware load."
    );

    Mempool mempool;
    const Ed25519SignatureProvider provider;

    const auto loaded =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            accountStateWithSenderBalance(1100),
            100,
            provider
        );

    requireCondition(
        loaded.loaded() &&
        loaded.loadedTransactionCount() == 1U &&
        mempool.size() == 1U,
        "Balance-aware persistent mempool load should accept sufficient balance."
    );

    clean(path);
}

void testLoadWithAccountStateRejectsInsufficientBalance() {
    const std::filesystem::path path =
        tempPath("insufficient-balance");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);
    initDirectory(directoryConfig);

    const Transaction transaction =
        signedTransfer("d", 1);

    requireCondition(
        PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            transactionKeyPair("d").publicKey(),
            kTimestamp + 50
        ).stored(),
        "Persistent transaction fixture should store before insufficient-balance load."
    );

    Mempool mempool;
    const Ed25519SignatureProvider provider;

    const auto loaded =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            accountStateWithSenderBalance(1099),
            100,
            provider
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.reason().find("balance is insufficient") != std::string::npos &&
        mempool.empty(),
        "Balance-aware persistent mempool load should reject insufficient balance."
    );

    clean(path);
}

void testMalformedPersistentMempoolFileIsRejected() {
    const std::filesystem::path path =
        tempPath("malformed");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);
    initDirectory(directoryConfig);

    const std::filesystem::path malformedPath =
        directoryConfig.mempoolDirectoryPath() / "tx_malformed.nodo";

    {
        std::ofstream output(malformedPath);
        output << "NODO_MEMPOOL_TRANSACTION_V2\n"
               << "transactionId=abc\n"
               << "acceptedAt=" << (kTimestamp + 40) << "\n"
               << "publicKeyMaterial=malformed-public-key\n"
               << "transaction=Transaction{bad}\n"
               << "unknownField=must-fail\n";
    }

    Mempool mempool;

    const auto loaded =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.reason().find("unknownField") != std::string::npos,
        "Malformed persistent mempool file should reject load."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testPersistLoadAndRemoveTransaction();
        testPersistIsIdempotent();
        testLoadWithAccountStateAcceptsSufficientBalance();
        testLoadWithAccountStateRejectsInsufficientBalance();
        testMalformedPersistentMempoolFileIsRejected();

        std::cout << "Nodo persistent mempool store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo persistent mempool store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
