#include "mempool/Mempool.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::core::AccountState;
using nodo::core::AccountStateView;
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

std::vector<std::string> explicitInputIds(
    const std::string& prefix,
    std::size_t count
) {
    std::vector<std::string> inputs;
    inputs.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        inputs.push_back(
            prefix + "_coin_lot_" + std::to_string(index) + "_padding_for_size"
        );
    }

    return inputs;
}

Transaction signedTransferWithInputs(
    const std::string& suffix,
    const std::string& from,
    const std::string& to,
    std::int64_t amountRaw,
    std::int64_t feeRaw,
    std::uint64_t nonce,
    std::int64_t timestamp,
    std::vector<std::string> inputCoinLotIds
) {
    Transaction transaction(
        TransactionType::TRANSFER,
        from,
        to,
        Amount::fromRawUnits(amountRaw),
        Amount::fromRawUnits(feeRaw),
        nonce,
        timestamp,
        std::move(inputCoinLotIds)
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

void testSameSenderNonceCanBeReplacedByHigherFee() {
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

    const auto replacement =
        mempool.admitTransaction(
            highFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 33
        );

    requireCondition(
        replacement.status() == MempoolAdmissionStatus::REPLACED,
        "Higher-fee transaction with same sender nonce should replace the old one."
    );

    requireCondition(
        !mempool.contains(lowFee.id()) &&
        mempool.contains(highFee.id()),
        "Mempool should keep the replacement transaction only."
    );

    requireCondition(
        mempool.size() == 1U,
        "Sender nonce replacement should keep mempool size stable."
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

void testTransactionsForBlockWithAccountStateRespectsNonceOrder() {
    Mempool mempool;

    const Transaction nonceEightHighFee =
        signedTransfer(
            "nonce-eight",
            "igor",
            "ana",
            1000,
            900,
            8,
            kTimestamp + 70
        );

    const Transaction nonceSevenLowFee =
        signedTransfer(
            "nonce-seven",
            "igor",
            "ana",
            1000,
            100,
            7,
            kTimestamp + 71
        );

    const Transaction nonceTenGap =
        signedTransfer(
            "nonce-ten",
            "igor",
            "ana",
            1000,
            1000,
            10,
            kTimestamp + 72
        );

    requireCondition(
        mempool.admitTransaction(
            nonceEightHighFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 73
        ).accepted(),
        "Future nonce transaction should enter the queue."
    );

    requireCondition(
        mempool.admitTransaction(
            nonceSevenLowFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 74
        ).accepted(),
        "Ready nonce transaction should enter the queue."
    );

    requireCondition(
        mempool.admitTransaction(
            nonceTenGap,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 75
        ).accepted(),
        "Gapped future nonce transaction should enter but wait."
    );

    AccountStateView view;
    requireCondition(
        view.putAccount(
            AccountState(
                "igor",
                Amount::fromRawUnits(100000),
                6
            )
        ),
        "Account state fixture should be valid."
    );

    const std::vector<Transaction> selected =
        mempool.transactionsForBlock(3, view);

    requireCondition(
        selected.size() == 2U,
        "Only contiguous executable nonces should be selected."
    );

    requireCondition(
        selected[0].id() == nonceSevenLowFee.id() &&
        selected[1].id() == nonceEightHighFee.id(),
        "Nonce-aware selection should execute sender transactions in nonce order."
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

void testMempoolSizeCapacityLimitAndEviction() {
    const Transaction tx1 =
        signedTransfer(
            "size-1",
            "igor",
            "ana",
            1000,
            100,
            1,
            kTimestamp
        );

    const Transaction tx2 =
        signedTransfer(
            "size-2",
            "ana",
            "igor",
            1000,
            200,
            1,
            kTimestamp + 1
        );

    const Transaction tx3 =
        signedTransfer(
            "size-3",
            "bob",
            "charlie",
            1000,
            50,
            1,
            kTimestamp + 2
        );

    std::size_t size1 = tx1.serialize().size();
    requireCondition(size1 > 0, "Serialized size of tx1 should be positive");

    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600,
            size1 + 10
        )
    );

    requireCondition(
        mempool.admitTransaction(
            tx1,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 10
        ).accepted(),
        "tx1 should be accepted"
    );

    requireCondition(
        mempool.currentMempoolSizeBytes() == size1,
        "Current size should equal tx1 size"
    );

    const auto result2 =
        mempool.admitTransaction(
            tx2,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        );

    requireCondition(
        result2.accepted(),
        "tx2 should be accepted, causing tx1 eviction"
    );

    requireCondition(
        !mempool.contains(tx1.id()),
        "tx1 should be evicted from mempool"
    );

    requireCondition(
        mempool.contains(tx2.id()),
        "tx2 should be in mempool"
    );

    const auto result3 =
        mempool.admitTransaction(
            tx3,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 12
        );

    requireCondition(
        result3.status() == MempoolAdmissionStatus::FEE_TOO_LOW,
        "tx3 should be rejected with FEE_TOO_LOW because it cannot evict higher-fee tx2"
    );

    requireCondition(
        mempool.isValid(
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ),
        "Mempool should remain valid after size eviction and rejection."
    );
}

void testSizeEvictionDryRunRejectLeavesMempoolUnchanged() {
    const Transaction lowFee =
        signedTransfer(
            "dry-low",
            "alice",
            "bob",
            1000,
            100,
            1,
            kTimestamp + 80
        );

    const Transaction midFee =
        signedTransfer(
            "dry-mid",
            "carol",
            "dave",
            1000,
            200,
            1,
            kTimestamp + 81
        );

    const Transaction incoming =
        signedTransferWithInputs(
            "dry-incoming",
            "erin",
            "frank",
            1000,
            150,
            1,
            kTimestamp + 82,
            explicitInputIds("dry_incoming", 20)
        );

    const std::size_t capacity =
        std::max(
            lowFee.serialize().size() + midFee.serialize().size(),
            incoming.serialize().size()
        ) + 10U;

    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600,
            capacity
        )
    );

    requireCondition(
        mempool.admitTransaction(
            lowFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 83
        ).accepted(),
        "Low-fee fixture should enter mempool."
    );

    requireCondition(
        mempool.admitTransaction(
            midFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 84
        ).accepted(),
        "Mid-fee fixture should enter mempool."
    );

    const std::size_t originalSize =
        mempool.currentMempoolSizeBytes();

    const auto result =
        mempool.admitTransaction(
            incoming,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 85
        );

    requireCondition(
        result.status() == MempoolAdmissionStatus::FEE_TOO_LOW,
        "Incoming tx should be rejected when the second required eviction has higher fee."
    );

    requireCondition(
        mempool.contains(lowFee.id()) &&
        mempool.contains(midFee.id()) &&
        !mempool.contains(incoming.id()),
        "Dry-run rejection must not remove existing transactions."
    );

    requireCondition(
        mempool.currentMempoolSizeBytes() == originalSize,
        "Dry-run rejection must preserve current mempool size."
    );

    requireCondition(
        mempool.isValid(
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ),
        "Mempool should remain valid after failed dry-run eviction."
    );
}

void testSizeEvictionDryRunAcceptsHigherFeeTransaction() {
    const Transaction lowFee =
        signedTransfer(
            "evict-low",
            "gail",
            "hank",
            1000,
            100,
            1,
            kTimestamp + 90
        );

    const Transaction midFee =
        signedTransfer(
            "evict-mid",
            "iris",
            "jules",
            1000,
            200,
            1,
            kTimestamp + 91
        );

    const Transaction incoming =
        signedTransferWithInputs(
            "evict-incoming",
            "kira",
            "liam",
            1000,
            500,
            1,
            kTimestamp + 92,
            explicitInputIds("evict_incoming", 20)
        );

    const std::size_t capacity =
        std::max(
            lowFee.serialize().size() + midFee.serialize().size(),
            incoming.serialize().size()
        ) + 10U;

    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600,
            capacity
        )
    );

    requireCondition(
        mempool.admitTransaction(
            lowFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 93
        ).accepted(),
        "Low-fee fixture should enter mempool."
    );

    requireCondition(
        mempool.admitTransaction(
            midFee,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 94
        ).accepted(),
        "Mid-fee fixture should enter mempool."
    );

    const auto result =
        mempool.admitTransaction(
            incoming,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 95
        );

    requireCondition(
        result.accepted(),
        "Higher-fee incoming tx should evict lower-fee txs and be accepted."
    );

    requireCondition(
        !mempool.contains(lowFee.id()) &&
        !mempool.contains(midFee.id()) &&
        mempool.contains(incoming.id()),
        "Incoming tx should replace the lower-fee size victims."
    );

    requireCondition(
        mempool.isValid(
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ),
        "Mempool should remain valid after successful size eviction."
    );
}

void testReplacementUpdatesMempoolSizeWithoutUnrelatedEviction() {
    const Transaction oldTransaction =
        signedTransfer(
            "replace-old",
            "mona",
            "nate",
            1000,
            100,
            1,
            kTimestamp + 100
        );

    const Transaction replacement =
        signedTransferWithInputs(
            "replace-new",
            "mona",
            "nate",
            1000,
            300,
            1,
            kTimestamp + 101,
            explicitInputIds("replace_new", 3)
        );

    Mempool mempool(
        MempoolConfig(
            10,
            0,
            true,
            3600,
            replacement.serialize().size() + 5U
        )
    );

    requireCondition(
        mempool.admitTransaction(
            oldTransaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 102
        ).accepted(),
        "Original transaction should enter mempool."
    );

    const auto result =
        mempool.admitTransaction(
            replacement,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 103
        );

    requireCondition(
        result.replaced(),
        "Higher-fee same-nonce transaction should replace the original."
    );

    requireCondition(
        !mempool.contains(oldTransaction.id()) &&
        mempool.contains(replacement.id()) &&
        mempool.size() == 1U,
        "Replacement should keep only the new transaction."
    );

    requireCondition(
        mempool.currentMempoolSizeBytes() == replacement.serialize().size(),
        "Replacement should update currentMempoolSizeBytes to the new serialized size."
    );

    requireCondition(
        mempool.isValid(
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION
        ),
        "Mempool should remain valid after replacement."
    );
}

} // namespace

int main() {
    try {
        testAdmitValidTransaction();
        testDuplicateTransactionIsSafeNoOp();
        testLowFeeRejected();
        testSameSenderNonceCanBeReplacedByHigherFee();
        testCapacityLimit();
        testTransactionsForBlockAreFeeOrdered();
        testTransactionsForBlockWithAccountStateRespectsNonceOrder();
        testPruneExpiredTransactions();
        testMempoolSizeCapacityLimitAndEviction();
        testSizeEvictionDryRunRejectLeavesMempoolUnchanged();
        testSizeEvictionDryRunAcceptsHigherFeeTransaction();
        testReplacementUpdatesMempoolSizeWithoutUnrelatedEviction();

        std::cout << "Nodo mempool tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo mempool tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
