#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/MempoolBlockProducer.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "mempool/Mempool.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::BlockProductionConfig;
using nodo::core::BlockProductionStatus;
using nodo::core::LedgerRecord;
using nodo::core::MempoolBlockProducer;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SigningDomain;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::mempool::Mempool;
using nodo::mempool::MempoolConfig;
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

KeyPair keyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicEd25519KeyPair(
        "mempool-producer-key-" + suffix
    );
}

Transaction signedTransfer(
    const std::string& suffix,
    const std::string& from,
    const std::string& to,
    std::int64_t feeRaw,
    std::uint64_t nonce,
    std::int64_t timestamp
) {
    Transaction transaction(
        TransactionType::TRANSFER,
        from,
        to,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(feeRaw),
        nonce,
        timestamp
    );

    const KeyPair key =
        keyPair(suffix);
    const Ed25519SignatureProvider provider;

    transaction.attachSignatureBundle(
        SignatureBundle::createSignature(
            transaction.signingPayload(),
            key.publicKey(),
            key.privateKeyForSigningOnly(),
            timestamp,
            provider,
            SigningDomain::USER_TRANSACTION
        )
    );

    return transaction;
}

ValidationWorkRecord validationWork(
    const std::string& evidence
) {
    return ValidationWorkRecord(
        "mempool-producer-bootstrap",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "mempool-producer-target-" + evidence,
        evidence,
        1,
        kTimestamp
    );
}

Blockchain blockchainWithGenesis() {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    validationWork("genesis"),
                    kTimestamp + 1
                )
            },
            kTimestamp + 2
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);

    return blockchain;
}

Mempool mempoolWithTwoTransactions() {
    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600
        )
    );

    const Transaction low =
        signedTransfer(
            "low",
            "igor",
            "ana",
            100,
            1,
            kTimestamp + 10
        );

    const Transaction high =
        signedTransfer(
            "high",
            "ana",
            "igor",
            500,
            2,
            kTimestamp + 11
        );

    requireCondition(
        mempool.admitTransaction(
            low,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 12
        ).accepted(),
        "Low fee transaction should enter mempool."
    );

    requireCondition(
        mempool.admitTransaction(
            high,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 13
        ).accepted(),
        "High fee transaction should enter mempool."
    );

    return mempool;
}

void testProducesCandidateBlockFromMempool() {
    const Blockchain blockchain =
        blockchainWithGenesis();

    const Mempool mempool =
        mempoolWithTwoTransactions();

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            blockchain,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(10, 1),
            kTimestamp + 20
        );

    // The stateless overload cannot produce non-genesis protocol blocks because
    // stateRoot and receiptsRoot cannot be correctly computed without a real
    // AccountStateView.  Producing a block with fake roots would be rejected by
    // every validating peer, so the security guard fires early.
    requireCondition(
        result.status() == BlockProductionStatus::BLOCK_AUDIT_FAILED,
        "Stateless overload must not produce a non-genesis protocol block."
    );

    requireCondition(
        result.reason().find("AccountStateView") != std::string::npos,
        "Rejection must explain that AccountStateView is required."
    );

    requireCondition(
        mempool.size() == 2U,
        "Block production rejection must not mutate the mempool."
    );
}

void testRejectsEmptyMempool() {
    const Blockchain blockchain =
        blockchainWithGenesis();

    const Mempool emptyMempool;

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            blockchain,
            emptyMempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(10, 1),
            kTimestamp + 30
        );

    requireCondition(
        result.status() == BlockProductionStatus::EMPTY_MEMPOOL,
        "Empty mempool should be rejected."
    );
}

void testRejectsInvalidBlockchain() {
    const Blockchain emptyBlockchain;
    const Mempool mempool =
        mempoolWithTwoTransactions();

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            emptyBlockchain,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(10, 1),
            kTimestamp + 40
        );

    requireCondition(
        result.status() == BlockProductionStatus::INVALID_BLOCKCHAIN,
        "Empty blockchain should be rejected."
    );
}

void testRejectsNotEnoughTransactions() {
    const Blockchain blockchain =
        blockchainWithGenesis();

    const Mempool mempool =
        mempoolWithTwoTransactions();

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            blockchain,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(10, 3),
            kTimestamp + 50
        );

    requireCondition(
        result.status() == BlockProductionStatus::NOT_ENOUGH_TRANSACTIONS,
        "Producer should reject when min transaction count is not met."
    );
}

void testRejectsInvalidConfig() {
    const Blockchain blockchain =
        blockchainWithGenesis();

    const Mempool mempool =
        mempoolWithTwoTransactions();

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            blockchain,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(0, 1),
            kTimestamp + 60
        );

    requireCondition(
        result.status() == BlockProductionStatus::INVALID_CONFIG,
        "Invalid production config should be rejected."
    );
}

void testProducedBlockHasNoPlaceholderRoots() {
    const Blockchain blockchain =
        blockchainWithGenesis();

    const Mempool mempool =
        mempoolWithTwoTransactions();

    const auto result =
        MempoolBlockProducer::produceCandidateBlock(
            blockchain,
            mempool,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            BlockProductionConfig(10, 1),
            kTimestamp + 20
        );

    // The stateless overload is now blocked from producing any non-genesis block.
    // This is the strongest guarantee that no placeholder or fake root value can
    // ever leak into a finalized protocol block via this code path.
    requireCondition(
        !result.produced() &&
        result.status() == BlockProductionStatus::BLOCK_AUDIT_FAILED,
        "Stateless overload must not produce any block at non-genesis height."
    );
}

} // namespace

int main() {
    try {
        testProducesCandidateBlockFromMempool();
        testRejectsEmptyMempool();
        testRejectsInvalidBlockchain();
        testRejectsNotEnoughTransactions();
        testRejectsInvalidConfig();
        testProducedBlockHasNoPlaceholderRoots();

        std::cout << "Nodo mempool block producer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo mempool block producer tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
