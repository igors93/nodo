#ifndef NODO_NODE_PERSISTENT_MEMPOOL_STORE_HPP
#define NODO_NODE_PERSISTENT_MEMPOOL_STORE_HPP

#include "core/AccountStateView.hpp"
#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureProvider.hpp"
#include "mempool/Mempool.hpp"
#include "node/NodeDataDirectory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class TransactionAdmissionContext;

enum class PersistentMempoolWriteStatus {
  STORED,
  ALREADY_STORED,
  INVALID_CONFIG,
  INVALID_TRANSACTION,
  IO_ERROR
};

std::string
persistentMempoolWriteStatusToString(PersistentMempoolWriteStatus status);

class PersistentMempoolWriteResult {
public:
  PersistentMempoolWriteResult();

  static PersistentMempoolWriteResult stored(std::string transactionId,
                                             std::filesystem::path path);

  static PersistentMempoolWriteResult alreadyStored(std::string transactionId,
                                                    std::filesystem::path path);

  static PersistentMempoolWriteResult
  rejected(PersistentMempoolWriteStatus status, std::string reason);

  PersistentMempoolWriteStatus status() const;
  const std::string &reason() const;
  const std::string &transactionId() const;
  const std::filesystem::path &path() const;

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

enum class PersistentMempoolLoadStatus { LOADED, INVALID_CONFIG, IO_ERROR };

class PersistentMempoolLoadResult {
public:
  PersistentMempoolLoadResult();

  static PersistentMempoolLoadResult
  loaded(std::size_t loadedTransactionCount,
         std::size_t skippedTransactionCount);

  static PersistentMempoolLoadResult
  rejected(PersistentMempoolLoadStatus status, std::string reason);

  PersistentMempoolLoadStatus status() const;
  const std::string &reason() const;
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
  struct DecodedGossipTransaction {
    core::Transaction transaction;
    std::int64_t acceptedAt;
  };

  // A transaction found in the persistent mempool directory that is not
  // yet present in the caller's live mempool, paired with its canonical
  // gossip-wire payload (identical format to serializeForGossip()).
  struct PendingLocalTransaction {
    core::Transaction transaction;
    std::string gossipPayload;
  };

  static PersistentMempoolWriteResult
  persistTransaction(const NodeDataDirectoryConfig &directoryConfig,
                     const core::Transaction &transaction,
                     const crypto::PublicKey &publicKey,
                     std::int64_t acceptedAt);

  static PersistentMempoolLoadResult
  loadIntoMempool(const NodeDataDirectoryConfig &directoryConfig,
                  mempool::Mempool &mempool, const crypto::CryptoPolicy &policy,
                  crypto::SecurityContext context);

  static PersistentMempoolLoadResult loadIntoMempool(
      const NodeDataDirectoryConfig &directoryConfig, mempool::Mempool &mempool,
      const crypto::CryptoPolicy &policy, crypto::SecurityContext context,
      const core::AccountStateView &accountStateView,
      std::int64_t minimumFeeRawUnits,
      const crypto::SignatureProvider &provider,
      const TransactionAdmissionContext *admissionContext = nullptr);

  static std::size_t
  removeTransactions(const NodeDataDirectoryConfig &directoryConfig,
                     const std::vector<std::string> &transactionIds);

  static std::filesystem::path
  transactionFilePath(const NodeDataDirectoryConfig &directoryConfig,
                      const std::string &transactionId);

  /*
   * Serialize a signed transaction to the gossip wire format (same as the
   * on-disk NODO_MEMPOOL_TRANSACTION_V3 schema). The transaction is
   * self-contained; the key parameter is checked against its signature.
   *
   * Returns an empty string if the transaction is unsigned or invalid.
   */
  static std::string serializeForGossip(const core::Transaction &transaction,
                                        const crypto::PublicKey &publicKey,
                                        std::int64_t acceptedAt);

  /*
   * Decode and authenticate a gossip payload. Admission remains the caller's
   * responsibility so network gossip uses TransactionAdmissionValidator with
   * the current account and protocol-domain state before touching the mempool.
   */
  static std::optional<DecodedGossipTransaction> deserializeGossip(
      const std::string &payload, const crypto::CryptoPolicy &policy,
      crypto::SecurityContext context, const std::string &expectedChainId);

  /*
   * Scans the persistent mempool directory for transactions not already
   * present in the given live mempool (e.g. a transaction a CLI command
   * just wrote to disk while this node's daemon was already running).
   * Files that fail to decode are skipped rather than aborting the scan.
   * The caller remains responsible for admission (TransactionAdmissionPolicy
   * / TransactionAdmissionValidator) before touching the mempool, exactly
   * as for deserializeGossip().
   */
  static std::vector<PendingLocalTransaction> collectTransactionsPendingGossip(
      const NodeDataDirectoryConfig &directoryConfig,
      const mempool::Mempool &mempool);

private:
  static std::string
  transactionFileContents(const core::Transaction &transaction,
                          const crypto::PublicKey &publicKey,
                          std::int64_t acceptedAt);

  static core::Transaction decodeTransactionFile(const std::string &contents);

  static std::int64_t decodeAcceptedAt(const std::string &contents);

  static void writeTextFile(const std::filesystem::path &path,
                            const std::string &contents);

  static std::string readTextFile(const std::filesystem::path &path);
};

} // namespace nodo::node

#endif
