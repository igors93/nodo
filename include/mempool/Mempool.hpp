#ifndef NODO_MEMPOOL_MEMPOOL_HPP
#define NODO_MEMPOOL_MEMPOOL_HPP

#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::mempool {

class MempoolConfig {
public:
    MempoolConfig();

    MempoolConfig(
        std::size_t maxTransactions,
        std::int64_t minimumFeeRaw,
        bool replaceByHigherFee,
        std::int64_t maxTransactionAgeSeconds
    );

    std::size_t maxTransactions() const;
    std::int64_t minimumFeeRaw() const;
    bool replaceByHigherFee() const;
    std::int64_t maxTransactionAgeSeconds() const;

    bool isValid() const;

private:
    std::size_t m_maxTransactions;
    std::int64_t m_minimumFeeRaw;
    bool m_replaceByHigherFee;
    std::int64_t m_maxTransactionAgeSeconds;
};

class MempoolEntry {
public:
    MempoolEntry(
        core::Transaction transaction,
        std::int64_t acceptedAt
    );

    const core::Transaction& transaction() const;
    std::int64_t acceptedAt() const;
    std::int64_t priorityFeeRaw() const;

    std::string senderNonceKey() const;

    bool isValid(
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context
    ) const;

    std::string serialize() const;

private:
    core::Transaction m_transaction;
    std::int64_t m_acceptedAt;
};

enum class MempoolAdmissionStatus {
    ACCEPTED,
    DUPLICATE,
    REPLACED,
    INVALID_CONFIG,
    INVALID_TRANSACTION,
    FEE_TOO_LOW,
    CAPACITY_REACHED,
    CONFLICTING_NONCE
};

std::string mempoolAdmissionStatusToString(
    MempoolAdmissionStatus status
);

class MempoolAdmissionResult {
public:
    MempoolAdmissionResult();

    static MempoolAdmissionResult accepted(
        std::string transactionId
    );

    static MempoolAdmissionResult duplicate(
        std::string transactionId
    );

    static MempoolAdmissionResult replaced(
        std::string transactionId,
        std::string replacedTransactionId
    );

    static MempoolAdmissionResult rejected(
        MempoolAdmissionStatus status,
        std::string reason
    );

    MempoolAdmissionStatus status() const;
    const std::string& reason() const;
    const std::string& transactionId() const;
    const std::string& replacedTransactionId() const;

    bool accepted() const;
    bool duplicate() const;
    bool replaced() const;
    bool success() const;

    std::string serialize() const;

private:
    MempoolAdmissionStatus m_status;
    std::string m_reason;
    std::string m_transactionId;
    std::string m_replacedTransactionId;
};

/*
 * Mempool is the local admission queue for transactions waiting to enter a
 * block.
 *
 * Security principle:
 * The mempool never mutates State. It only admits structurally valid,
 * policy-compatible, non-duplicated transactions into a deterministic queue.
 */
class Mempool {
public:
    explicit Mempool(
        MempoolConfig config = MempoolConfig()
    );

    MempoolAdmissionResult admitTransaction(
        const core::Transaction& transaction,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        std::int64_t acceptedAt
    );

    bool contains(
        const std::string& transactionId
    ) const;

    const MempoolEntry* entry(
        const std::string& transactionId
    ) const;

    bool removeTransaction(
        const std::string& transactionId
    );

    std::size_t pruneExpired(
        std::int64_t currentTime
    );

    std::vector<core::Transaction> transactionsForBlock(
        std::size_t maxCount
    ) const;

    std::size_t size() const;
    bool empty() const;

    bool isValid(
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context
    ) const;

    std::string serialize() const;

private:
    MempoolConfig m_config;
    std::map<std::string, MempoolEntry> m_entriesById;
    std::map<std::string, std::string> m_transactionIdBySenderNonce;

    static std::string senderNonceKey(
        const core::Transaction& transaction
    );

    void removeIndexesForEntry(
        const MempoolEntry& entry
    );
};

} // namespace nodo::mempool

#endif
