#ifndef NODO_NODE_PERSISTENT_MEMPOOL_STORE_HPP
#define NODO_NODE_PERSISTENT_MEMPOOL_STORE_HPP

#include "core/Transaction.hpp"
#include "core/AccountStateView.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureProvider.hpp"
#include "mempool/Mempool.hpp"
#include "node/NodeDataDirectory.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::node {

enum class PersistentMempoolWriteStatus {
    STORED,
    ALREADY_STORED,
    INVALID_CONFIG,
    INVALID_TRANSACTION,
    IO_ERROR
};

std::string persistentMempoolWriteStatusToString(
    PersistentMempoolWriteStatus status
);

class PersistentMempoolWriteResult {
public:
    PersistentMempoolWriteResult();

    static PersistentMempoolWriteResult stored(
        std::string transactionId,
        std::filesystem::path path
    );

    static PersistentMempoolWriteResult alreadyStored(
        std::string transactionId,
        std::filesystem::path path
    );

    static PersistentMempoolWriteResult rejected(
        PersistentMempoolWriteStatus status,
        std::string reason
    );

    PersistentMempoolWriteStatus status() const;
    const std::string& reason() const;
    const std::string& transactionId() const;
    const std::filesystem::path& path() const;

    bool stored() const;
    bool alreadyStored() const;
    bool success() const;

    std::string serialize() const;

private:
    PersistentMempoolWriteStatus m_status;
    std::string m_reason;
    std::string m_transactionId;
    std::filesystem::path m_path;
};

enum class PersistentMempoolLoadStatus {
    LOADED,
    INVALID_CONFIG,
    IO_ERROR
};

class PersistentMempoolLoadResult {
public:
    PersistentMempoolLoadResult();

    static PersistentMempoolLoadResult loaded(
        std::size_t loadedTransactionCount,
        std::size_t skippedTransactionCount
    );

    static PersistentMempoolLoadResult rejected(
        PersistentMempoolLoadStatus status,
        std::string reason
    );

    PersistentMempoolLoadStatus status() const;
    const std::string& reason() const;
    std::size_t loadedTransactionCount() const;
    std::size_t skippedTransactionCount() const;

    bool loaded() const;
    std::string serialize() const;

private:
    PersistentMempoolLoadStatus m_status;
    std::string m_reason;
    std::size_t m_loadedTransactionCount;
    std::size_t m_skippedTransactionCount;
};

class PersistentMempoolStore {
public:
    static PersistentMempoolWriteResult persistTransaction(
        const NodeDataDirectoryConfig& directoryConfig,
        const core::Transaction& transaction,
        const crypto::PublicKey& publicKey,
        std::int64_t acceptedAt
    );

    static PersistentMempoolLoadResult loadIntoMempool(
        const NodeDataDirectoryConfig& directoryConfig,
        mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context
    );

    static PersistentMempoolLoadResult loadIntoMempool(
        const NodeDataDirectoryConfig& directoryConfig,
        mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const core::AccountStateView& accountStateView,
        std::int64_t minimumFeeRawUnits,
        const crypto::SignatureProvider& provider
    );

    static std::size_t removeTransactions(
        const NodeDataDirectoryConfig& directoryConfig,
        const std::vector<std::string>& transactionIds
    );

    static std::filesystem::path transactionFilePath(
        const NodeDataDirectoryConfig& directoryConfig,
        const std::string& transactionId
    );

private:
    static std::string transactionFileContents(
        const core::Transaction& transaction,
        const crypto::PublicKey& publicKey,
        std::int64_t acceptedAt
    );

    static core::Transaction decodeTransactionFile(
        const std::string& contents
    );

    static std::int64_t decodeAcceptedAt(
        const std::string& contents
    );

    static void writeTextFile(
        const std::filesystem::path& path,
        const std::string& contents
    );

    static std::string readTextFile(
        const std::filesystem::path& path
    );
};

} // namespace nodo::node

#endif
