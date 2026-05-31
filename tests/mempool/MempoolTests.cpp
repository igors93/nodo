#include "mempool/Mempool.hpp"
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
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
using nodo::mempool::Mempool;
using nodo::mempool::MempoolAdmissionStatus;
using nodo::mempool::MempoolConfig;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

KeyPair keyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicEd25519KeyPair(
        "mempool-key-" + suffix
    );
}

Transaction signedTransfer(
    const std::string& suffix,
    const std::string& from,
    const std::string& to,
    std::int64_t amountRaw,
    std::int64_t feeRaw,
    std::uint64_t nonce,
    std::int64_t timestamp
) {
    Transaction transaction(
        TransactionType::TRANSFER,
        from,
        to,
        Amount::fromRawUnits(amountRaw),
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

void testAdmitValidTransaction() {
    Mempool mempool(
        MempoolConfig(
            10,
            100,
            true,
            3600
        )
    );

    const Transaction transaction =
        signedTransfer(
            "a",
            "igor",
            "ana",
            1000,
            150,
            1,
            kTimestamp
        );

    const auto result =
        mempool.admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 1
        );

    requireCondition(
        result.accepted(),
        "Valid transaction should be accepted."
    );

    requireCondition(
        mempool.contains(transaction.id()),
        "Accepted transaction should be present in mempool."
    );

    requireCondition(
        mempool.isValid(
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ),
        "Mempool should remain valid after admission."
    );
}

void testDuplicateTransactionIsSafeNoOp() {
    Mempool mempool;

    const Transaction transaction =
        signedTransfer(
            "b",
            "igor",
            "ana",
            1000,
            100,
            2,
            kTimestamp + 10
        );

    requireCondition(
        mempool.admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "Initial transaction should be accepted."
    );

    const auto duplicate =
        mempool.admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 12
        );

    requireCondition(
        duplicate.duplicate(),
        "Duplicate transaction should be reported as duplicate."
    );

    requireCondition(
        mempool.size() == 1U,
        "Duplicate transaction should not increase mempool size."
    );
}

void testLowFeeRejected() {
    Mempool mempool(
        MempoolConfig(
            10,
            500,
            true,
            3600
        )
    );

    const Transaction transaction =
        signedTransfer(
            "c",
            "igor",
            "ana",
            1000,
            100,
            3,
            kTimestamp + 20
        );

    const auto result =
        mempool.admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 21
        );

    requireCondition(
        result.status() == MempoolAdmissionStatus::FEE_TOO_LOW,
        "Low-fee transaction should be rejected."
    );

    requireCondition(
        mempool.empty(),
        "Rejected low-fee transaction should not enter mempool."
    );
}

void testSameSenderNonceIsRejected() {
    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600
        )
    );

    const Transaction lowFee =
        signedTransfer(
            "d-low",
            "igor",
            "ana",
            1000,
            100,
            4,
            kTimestamp + 30
        );

    const Transaction highFee =
        signedTransfer(
            "d-high",
            "igor",
            "ana",
            1000,
            300,
            4,
            kTimestamp + 31
        );

    requireCondition(
        mempool.admitTransaction(
            lowFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 32
        ).accepted(),
        "Low-fee transaction should be accepted first."
    );

    const auto conflict =
        mempool.admitTransaction(
            highFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 33
        );

    requireCondition(
        conflict.status() == MempoolAdmissionStatus::CONFLICTING_NONCE,
        "Transaction with same sender nonce should be rejected."
    );

    requireCondition(
        mempool.contains(lowFee.id()) &&
        !mempool.contains(highFee.id()),
        "Mempool should keep the original transaction only."
    );

    requireCondition(
        mempool.size() == 1U,
        "Rejected sender nonce conflict should keep mempool size stable."
    );
}

void testCapacityLimit() {
    Mempool mempool(
        MempoolConfig(
            1,
            0,
            true,
            3600
        )
    );

    const Transaction first =
        signedTransfer(
            "e1",
            "igor",
            "ana",
            1000,
            100,
            5,
            kTimestamp + 40
        );

    const Transaction second =
        signedTransfer(
            "e2",
            "ana",
            "igor",
            1000,
            100,
            6,
            kTimestamp + 41
        );

    requireCondition(
        mempool.admitTransaction(
            first,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 42
        ).accepted(),
        "First transaction should be accepted."
    );

    const auto result =
        mempool.admitTransaction(
            second,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 43
        );

    requireCondition(
        result.status() == MempoolAdmissionStatus::CAPACITY_REACHED,
        "Second transaction should be rejected by capacity limit."
    );
}

void testTransactionsForBlockAreFeeOrdered() {
    Mempool mempool;

    const Transaction low =
        signedTransfer(
            "f-low",
            "igor",
            "ana",
            1000,
            100,
            7,
            kTimestamp + 50
        );

    const Transaction high =
        signedTransfer(
            "f-high",
            "ana",
            "igor",
            1000,
            500,
            8,
            kTimestamp + 51
        );

    requireCondition(
        mempool.admitTransaction(
            low,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 52
        ).accepted(),
        "Low-fee transaction should be accepted."
    );

    requireCondition(
        mempool.admitTransaction(
            high,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 53
        ).accepted(),
        "High-fee transaction should be accepted."
    );

    const std::vector<Transaction> selected =
        mempool.transactionsForBlock(2);

    requireCondition(
        selected.size() == 2U,
        "Two transactions should be selected."
    );

    requireCondition(
        selected.front().id() == high.id(),
        "Higher-fee transaction should be selected first."
    );
}

void testPruneExpiredTransactions() {
    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            10
        )
    );

    const Transaction transaction =
        signedTransfer(
            "g",
            "igor",
            "ana",
            1000,
            100,
            9,
            kTimestamp + 60
        );

    requireCondition(
        mempool.admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 61
        ).accepted(),
        "Transaction should be accepted before pruning."
    );

    const std::size_t pruned =
        mempool.pruneExpired(kTimestamp + 100);

    requireCondition(
        pruned == 1U,
        "Expired transaction should be pruned."
    );

    requireCondition(
        mempool.empty(),
        "Mempool should be empty after pruning expired transaction."
    );
}

} // namespace

int main() {
    try {
        testAdmitValidTransaction();
        testDuplicateTransactionIsSafeNoOp();
        testLowFeeRejected();
        testSameSenderNonceIsRejected();
        testCapacityLimit();
        testTransactionsForBlockAreFeeOrdered();
        testPruneExpiredTransactions();

        std::cout << "Nodo mempool tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo mempool tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
