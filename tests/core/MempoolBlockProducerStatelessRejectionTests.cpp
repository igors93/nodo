#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/MempoolBlockProducer.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "mempool/Mempool.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::BlockProductionConfig;
using nodo::core::BlockProductionStatus;
using nodo::core::LedgerRecord;
using nodo::core::MempoolBlockProducer;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
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

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Transaction signedTransfer(const std::string& suffix, std::uint64_t nonce) {
    Transaction tx(
        TransactionType::TRANSFER,
        "stateless-sender-" + suffix,
        "stateless-recipient-" + suffix,
        Amount::fromRawUnits(100),
        Amount::fromRawUnits(10),
        nonce,
        kTimestamp + static_cast<std::int64_t>(nonce)
    );

    const KeyPair kp = KeyPair::createDeterministicEd25519KeyPair("stateless-key-" + suffix);
    const Ed25519SignatureProvider provider;
    tx.attachSignatureBundle(
        SignatureBundle::createSignature(
            tx.signingPayload(),
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            tx.timestamp(),
            provider,
            SigningDomain::USER_TRANSACTION
        )
    );
    return tx;
}

ValidationWorkRecord workRecord(const std::string& id) {
    return ValidationWorkRecord(
        "stateless-test-validator",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "stateless-test-target-" + id,
        id,
        1,
        kTimestamp
    );
}

Blockchain blockchainWithGenesis() {
    Blockchain blockchain;
    blockchain.addGenesisBlock(
        Block::createGenesisBlock(
            {LedgerRecord::fromValidationWorkRecord(workRecord("genesis"), kTimestamp + 1)},
            kTimestamp + 2
        )
    );
    return blockchain;
}

Mempool mempoolWithTransaction() {
    Mempool mempool(MempoolConfig(10, 0, true, 3600));
    const Transaction tx = signedTransfer("a", 1);
    mempool.admitTransaction(
        tx,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        kTimestamp + 10
    );
    return mempool;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testStatelessOverloadRejectsNonGenesisBlock() {
    const Blockchain blockchain = blockchainWithGenesis();
    const Mempool mempool = mempoolWithTransaction();

    const auto result = MempoolBlockProducer::produceCandidateBlock(
        blockchain,
        mempool,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        BlockProductionConfig(10, 1),
        kTimestamp + 20
    );

    requireCondition(
        result.status() == BlockProductionStatus::BLOCK_AUDIT_FAILED,
        "Stateless overload must reject non-genesis block with BLOCK_AUDIT_FAILED."
    );
    requireCondition(
        !result.produced(),
        "Stateless overload must not produce a block."
    );
}

void testStatelessRejectionReasonMentionsAccountStateView() {
    const Blockchain blockchain = blockchainWithGenesis();
    const Mempool mempool = mempoolWithTransaction();

    const auto result = MempoolBlockProducer::produceCandidateBlock(
        blockchain,
        mempool,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        BlockProductionConfig(10, 1),
        kTimestamp + 20
    );

    requireCondition(
        result.reason().find("AccountStateView") != std::string::npos,
        "Rejection reason must explain that AccountStateView is required."
    );
}

void testStatelessOverloadStillRejectsEmptyMempool() {
    const Blockchain blockchain = blockchainWithGenesis();
    const Mempool emptyMempool;

    const auto result = MempoolBlockProducer::produceCandidateBlock(
        blockchain,
        emptyMempool,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        BlockProductionConfig(10, 1),
        kTimestamp + 20
    );

    // Empty mempool is rejected before the AccountStateView guard.
    requireCondition(
        result.status() == BlockProductionStatus::EMPTY_MEMPOOL,
        "Empty mempool must be rejected before AccountStateView guard fires."
    );
}

void testStatelessOverloadStillRejectsEmptyBlockchain() {
    const Blockchain emptyBlockchain;
    const Mempool mempool = mempoolWithTransaction();

    const auto result = MempoolBlockProducer::produceCandidateBlock(
        emptyBlockchain,
        mempool,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        BlockProductionConfig(10, 1),
        kTimestamp + 20
    );

    // Empty blockchain is rejected before the AccountStateView guard.
    requireCondition(
        result.status() == BlockProductionStatus::INVALID_BLOCKCHAIN,
        "Empty blockchain must be rejected before AccountStateView guard fires."
    );
}

void testStatelessOverloadMempoolNotMutatedOnRejection() {
    const Blockchain blockchain = blockchainWithGenesis();
    const Mempool mempool = mempoolWithTransaction();

    const std::size_t sizeBefore = mempool.size();

    MempoolBlockProducer::produceCandidateBlock(
        blockchain,
        mempool,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        BlockProductionConfig(10, 1),
        kTimestamp + 20
    );

    requireCondition(
        mempool.size() == sizeBefore,
        "Rejected block production must not mutate the mempool."
    );
}

} // namespace

int main() {
    try {
        testStatelessOverloadRejectsNonGenesisBlock();
        testStatelessRejectionReasonMentionsAccountStateView();
        testStatelessOverloadStillRejectsEmptyMempool();
        testStatelessOverloadStillRejectsEmptyBlockchain();
        testStatelessOverloadMempoolNotMutatedOnRejection();

        std::cout << "MempoolBlockProducer stateless rejection tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
