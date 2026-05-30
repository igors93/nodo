#include "mempool/Mempool.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace nodo::mempool {

MempoolConfig::MempoolConfig()
    : m_maxTransactions(5000),
      m_minimumFeeRaw(0),
      m_replaceByHigherFee(true),
      m_maxTransactionAgeSeconds(60 * 60) {}

MempoolConfig::MempoolConfig(
    std::size_t maxTransactions,
    std::int64_t minimumFeeRaw,
    bool replaceByHigherFee,
    std::int64_t maxTransactionAgeSeconds
)
    : m_maxTransactions(maxTransactions),
      m_minimumFeeRaw(minimumFeeRaw),
      m_replaceByHigherFee(replaceByHigherFee),
      m_maxTransactionAgeSeconds(maxTransactionAgeSeconds) {}

std::size_t MempoolConfig::maxTransactions() const {
    return m_maxTransactions;
}

std::int64_t MempoolConfig::minimumFeeRaw() const {
    return m_minimumFeeRaw;
}

bool MempoolConfig::replaceByHigherFee() const {
    return m_replaceByHigherFee;
}

std::int64_t MempoolConfig::maxTransactionAgeSeconds() const {
    return m_maxTransactionAgeSeconds;
}

bool MempoolConfig::isValid() const {
    return m_maxTransactions > 0 &&
           m_minimumFeeRaw >= 0 &&
           m_maxTransactionAgeSeconds > 0;
}

MempoolEntry::MempoolEntry(
    core::Transaction transaction,
    std::int64_t acceptedAt
)
    : m_transaction(std::move(transaction)),
      m_acceptedAt(acceptedAt) {}

const core::Transaction& MempoolEntry::transaction() const {
    return m_transaction;
}

std::int64_t MempoolEntry::acceptedAt() const {
    return m_acceptedAt;
}

std::int64_t MempoolEntry::priorityFeeRaw() const {
    return m_transaction.fee().rawUnits();
}

std::string MempoolEntry::senderNonceKey() const {
    return m_transaction.fromAddress()
        + "#"
        + std::to_string(m_transaction.nonce());
}

bool MempoolEntry::isValid(
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context
) const {
    if (m_acceptedAt <= 0) {
        return false;
    }

    return m_transaction.isStructurallyValid(
        policy,
        context
    );
}

std::string MempoolEntry::serialize() const {
    std::ostringstream oss;

    oss << "MempoolEntry{"
        << "acceptedAt=" << m_acceptedAt
        << ";priorityFeeRaw=" << priorityFeeRaw()
        << ";transaction=" << m_transaction.serialize()
        << "}";

    return oss.str();
}

std::string mempoolAdmissionStatusToString(
    MempoolAdmissionStatus status
) {
    switch (status) {
        case MempoolAdmissionStatus::ACCEPTED:
            return "ACCEPTED";
        case MempoolAdmissionStatus::DUPLICATE:
            return "DUPLICATE";
        case MempoolAdmissionStatus::REPLACED:
            return "REPLACED";
        case MempoolAdmissionStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case MempoolAdmissionStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case MempoolAdmissionStatus::FEE_TOO_LOW:
            return "FEE_TOO_LOW";
        case MempoolAdmissionStatus::CAPACITY_REACHED:
            return "CAPACITY_REACHED";
        case MempoolAdmissionStatus::CONFLICTING_NONCE:
            return "CONFLICTING_NONCE";
        default:
            return "INVALID_TRANSACTION";
    }
}

MempoolAdmissionResult::MempoolAdmissionResult()
    : m_status(MempoolAdmissionStatus::INVALID_TRANSACTION),
      m_reason("Uninitialized mempool admission result."),
      m_transactionId(""),
      m_replacedTransactionId("") {}

MempoolAdmissionResult MempoolAdmissionResult::accepted(
    std::string transactionId
) {
    MempoolAdmissionResult result;
    result.m_status = MempoolAdmissionStatus::ACCEPTED;
    result.m_transactionId = std::move(transactionId);
    return result;
}

MempoolAdmissionResult MempoolAdmissionResult::duplicate(
    std::string transactionId
) {
    MempoolAdmissionResult result;
    result.m_status = MempoolAdmissionStatus::DUPLICATE;
    result.m_reason = "Transaction already exists in mempool.";
    result.m_transactionId = std::move(transactionId);
    return result;
}

MempoolAdmissionResult MempoolAdmissionResult::replaced(
    std::string transactionId,
    std::string replacedTransactionId
) {
    MempoolAdmissionResult result;
    result.m_status = MempoolAdmissionStatus::REPLACED;
    result.m_transactionId = std::move(transactionId);
    result.m_replacedTransactionId = std::move(replacedTransactionId);
    return result;
}

MempoolAdmissionResult MempoolAdmissionResult::rejected(
    MempoolAdmissionStatus status,
    std::string reason
) {
    MempoolAdmissionResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

MempoolAdmissionStatus MempoolAdmissionResult::status() const {
    return m_status;
}

const std::string& MempoolAdmissionResult::reason() const {
    return m_reason;
}

const std::string& MempoolAdmissionResult::transactionId() const {
    return m_transactionId;
}

const std::string& MempoolAdmissionResult::replacedTransactionId() const {
    return m_replacedTransactionId;
}

bool MempoolAdmissionResult::accepted() const {
    return m_status == MempoolAdmissionStatus::ACCEPTED;
}

bool MempoolAdmissionResult::duplicate() const {
    return m_status == MempoolAdmissionStatus::DUPLICATE;
}

bool MempoolAdmissionResult::replaced() const {
    return m_status == MempoolAdmissionStatus::REPLACED;
}

bool MempoolAdmissionResult::success() const {
    return accepted() || duplicate() || replaced();
}

std::string MempoolAdmissionResult::serialize() const {
    std::ostringstream oss;

    oss << "MempoolAdmissionResult{"
        << "status=" << mempoolAdmissionStatusToString(m_status)
        << ";reason=" << m_reason
        << ";transactionId=" << m_transactionId
        << ";replacedTransactionId=" << m_replacedTransactionId
        << "}";

    return oss.str();
}

Mempool::Mempool(
    MempoolConfig config
)
    : m_config(config),
      m_entriesById(),
      m_transactionIdBySenderNonce() {}

MempoolAdmissionResult Mempool::admitTransaction(
    const core::Transaction& transaction,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    std::int64_t acceptedAt
) {
    if (!m_config.isValid()) {
        return MempoolAdmissionResult::rejected(
            MempoolAdmissionStatus::INVALID_CONFIG,
            "Mempool configuration is invalid."
        );
    }

    if (acceptedAt <= 0) {
        return MempoolAdmissionResult::rejected(
            MempoolAdmissionStatus::INVALID_TRANSACTION,
            "Mempool acceptedAt timestamp must be positive."
        );
    }

    if (!transaction.isStructurallyValid(
            policy,
            context
        )) {
        return MempoolAdmissionResult::rejected(
            MempoolAdmissionStatus::INVALID_TRANSACTION,
            "Transaction failed structural or policy validation."
        );
    }

    if (transaction.fee().rawUnits() < m_config.minimumFeeRaw()) {
        return MempoolAdmissionResult::rejected(
            MempoolAdmissionStatus::FEE_TOO_LOW,
            "Transaction fee is below mempool minimum."
        );
    }

    if (contains(transaction.id())) {
        return MempoolAdmissionResult::duplicate(
            transaction.id()
        );
    }

    const std::string senderNonce =
        senderNonceKey(transaction);

    const auto existingByNonce =
        m_transactionIdBySenderNonce.find(senderNonce);

    if (existingByNonce != m_transactionIdBySenderNonce.end()) {
        const std::string existingTransactionId =
            existingByNonce->second;

        const auto existingEntry =
            m_entriesById.find(existingTransactionId);

        if (existingEntry == m_entriesById.end()) {
            return MempoolAdmissionResult::rejected(
                MempoolAdmissionStatus::INVALID_CONFIG,
                "Mempool nonce index is inconsistent."
            );
        }

        if (!m_config.replaceByHigherFee() ||
            transaction.fee().rawUnits() <=
                existingEntry->second.transaction().fee().rawUnits()) {
            return MempoolAdmissionResult::rejected(
                MempoolAdmissionStatus::CONFLICTING_NONCE,
                "Another transaction with the same sender nonce is already in mempool."
            );
        }

        removeIndexesForEntry(existingEntry->second);
        m_entriesById.erase(existingEntry);

        MempoolEntry newEntry(
            transaction,
            acceptedAt
        );

        m_transactionIdBySenderNonce.emplace(
            newEntry.senderNonceKey(),
            transaction.id()
        );

        m_entriesById.emplace(
            transaction.id(),
            newEntry
        );

        return MempoolAdmissionResult::replaced(
            transaction.id(),
            existingTransactionId
        );
    }

    if (m_entriesById.size() >= m_config.maxTransactions()) {
        return MempoolAdmissionResult::rejected(
            MempoolAdmissionStatus::CAPACITY_REACHED,
            "Mempool capacity reached."
        );
    }

    MempoolEntry entry(
        transaction,
        acceptedAt
    );

    m_transactionIdBySenderNonce.emplace(
        entry.senderNonceKey(),
        transaction.id()
    );

    m_entriesById.emplace(
        transaction.id(),
        entry
    );

    return MempoolAdmissionResult::accepted(
        transaction.id()
    );
}

bool Mempool::contains(
    const std::string& transactionId
) const {
    return m_entriesById.find(transactionId) != m_entriesById.end();
}

const MempoolEntry* Mempool::entry(
    const std::string& transactionId
) const {
    const auto found =
        m_entriesById.find(transactionId);

    if (found == m_entriesById.end()) {
        return nullptr;
    }

    return &found->second;
}

bool Mempool::removeTransaction(
    const std::string& transactionId
) {
    auto found =
        m_entriesById.find(transactionId);

    if (found == m_entriesById.end()) {
        return false;
    }

    removeIndexesForEntry(found->second);
    m_entriesById.erase(found);

    return true;
}

std::size_t Mempool::pruneExpired(
    std::int64_t currentTime
) {
    if (currentTime <= 0) {
        return 0;
    }

    std::vector<std::string> expiredIds;

    for (const auto& [transactionId, entry] : m_entriesById) {
        if (currentTime > entry.acceptedAt() &&
            currentTime - entry.acceptedAt() > m_config.maxTransactionAgeSeconds()) {
            expiredIds.push_back(transactionId);
        }
    }

    for (const auto& transactionId : expiredIds) {
        removeTransaction(transactionId);
    }

    return expiredIds.size();
}

std::vector<core::Transaction> Mempool::transactionsForBlock(
    std::size_t maxCount
) const {
    std::vector<MempoolEntry> entries;

    for (const auto& [_, entry] : m_entriesById) {
        entries.push_back(entry);
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const MempoolEntry& left, const MempoolEntry& right) {
            if (left.priorityFeeRaw() != right.priorityFeeRaw()) {
                return left.priorityFeeRaw() > right.priorityFeeRaw();
            }

            if (left.acceptedAt() != right.acceptedAt()) {
                return left.acceptedAt() < right.acceptedAt();
            }

            return left.transaction().id() < right.transaction().id();
        }
    );

    std::vector<core::Transaction> transactions;

    for (const auto& entry : entries) {
        if (transactions.size() >= maxCount) {
            break;
        }

        transactions.push_back(entry.transaction());
    }

    return transactions;
}

std::size_t Mempool::size() const {
    return m_entriesById.size();
}

bool Mempool::empty() const {
    return m_entriesById.empty();
}

bool Mempool::isValid(
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context
) const {
    if (!m_config.isValid()) {
        return false;
    }

    if (m_entriesById.size() != m_transactionIdBySenderNonce.size()) {
        return false;
    }

    for (const auto& [transactionId, entry] : m_entriesById) {
        if (transactionId != entry.transaction().id()) {
            return false;
        }

        if (!entry.isValid(
                policy,
                context
            )) {
            return false;
        }

        const auto indexed =
            m_transactionIdBySenderNonce.find(entry.senderNonceKey());

        if (indexed == m_transactionIdBySenderNonce.end() ||
            indexed->second != transactionId) {
            return false;
        }
    }

    return true;
}

std::string Mempool::serialize() const {
    std::ostringstream oss;

    oss << "Mempool{"
        << "size=" << m_entriesById.size()
        << ";entries=[";

    bool first = true;

    for (const auto& [_, entry] : m_entriesById) {
        if (!first) {
            oss << ",";
        }

        oss << entry.serialize();
        first = false;
    }

    oss << "]}";

    return oss.str();
}

std::string Mempool::senderNonceKey(
    const core::Transaction& transaction
) {
    return transaction.fromAddress()
        + "#"
        + std::to_string(transaction.nonce());
}

void Mempool::removeIndexesForEntry(
    const MempoolEntry& entry
) {
    m_transactionIdBySenderNonce.erase(
        entry.senderNonceKey()
    );
}

} // namespace nodo::mempool
