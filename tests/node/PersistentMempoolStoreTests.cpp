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

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::filesystem::path tempPath(const std::string &suffix) {
  return std::filesystem::temp_directory_path() /
         ("nodo-persistent-mempool-tests-" + suffix);
}

void clean(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

KeyPair validatorKeyPair(const std::string &suffix) {
  return KeyPair::createDeterministicBls12381KeyPair(
      "persistent-mempool-validator-key-" + suffix);
}

PublicKey validatorPublicKey(const std::string &suffix) {
  return validatorKeyPair(suffix).publicKey();
}

KeyPair transactionKeyPair(const std::string &suffix) {
  (void)suffix;
  return KeyPair::createDeterministicEd25519KeyPair(
      "persistent-mempool-transaction-key");
}

BootstrapValidatorConfig validator(const std::string &suffix) {
  return BootstrapValidatorConfig(validatorPublicKey(suffix), 1, 1,
                                  "persistent-mempool-validator-" + suffix);
}

GenesisConfig genesisConfig() {
  return GenesisConfig(NetworkParameters::developmentLocal(), kTimestamp,
                       {validator("a"), validator("b")},
                       "persistent-mempool-genesis");
}

PeerInfo localPeer() {
  return PeerInfo("persistent-mempool-peer", "127.0.0.1:9700", "nodo/0.1", 0,
                  kTimestamp);
}

Transaction signedTransfer(const std::string &suffix, std::uint64_t nonce) {
  const KeyPair keyPair = transactionKeyPair(suffix);
  Transaction transaction(TransactionType::TRANSFER, keyPair.address().value(),
                          "persistent-mempool-recipient",
                          Amount::fromRawUnits(1000), Amount::fromRawUnits(100),
                          nonce, kTimestamp + static_cast<std::int64_t>(nonce));

  const Ed25519SignatureProvider provider;

  transaction.withChainId("nodo-localnet-1");

  transaction.attachSignatureBundle(SignatureBundle::createSignature(
      transaction.signingPayload(), keyPair.publicKey(),
      keyPair.privateKeyForSigningOnly(), transaction.timestamp(), provider,
      SigningDomain::USER_TRANSACTION));

  return transaction;
}

AccountStateView accountStateWithSenderBalance(std::int64_t balanceRawUnits) {
  AccountStateView view;

  requireCondition(view.putAccount(AccountState(
                       transactionKeyPair("state").address().value(),
                       Amount::fromRawUnits(balanceRawUnits), 0)),
                   "Account state test fixture should accept sender account.");

  return view;
}

void initDirectory(const NodeDataDirectoryConfig &directoryConfig) {
  requireCondition(NodeDataDirectory::initialize(directoryConfig,
                                                 genesisConfig(), localPeer(),
                                                 kTimestamp + 1)
                       .initialized(),
                   "Data directory should initialize.");
}

void testPersistLoadAndRemoveTransaction() {
  const std::filesystem::path path = tempPath("store-load-remove");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction transaction = signedTransfer("a", 1);

  const auto persisted = PersistentMempoolStore::persistTransaction(
      directoryConfig, transaction, transactionKeyPair("a").publicKey(),
      kTimestamp + 20);

  requireCondition(persisted.stored(), "Persistent transaction should store.");

  Mempool mempool;

  const auto loaded = PersistentMempoolStore::loadIntoMempool(
      directoryConfig, mempool, CryptoPolicy::developmentPolicy(),
      SecurityContext::USER_TRANSACTION);

  requireCondition(loaded.loaded() && loaded.loadedTransactionCount() == 1U &&
                       mempool.size() == 1U,
                   "Persistent transaction should load into mempool.");

  requireCondition(PersistentMempoolStore::removeTransactions(
                       directoryConfig, {transaction.id()}) == 1U,
                   "Persistent transaction should be removed.");

  Mempool emptyMempool;

  requireCondition(
      PersistentMempoolStore::loadIntoMempool(directoryConfig, emptyMempool,
                                              CryptoPolicy::developmentPolicy(),
                                              SecurityContext::USER_TRANSACTION)
              .loadedTransactionCount() == 0U,
      "Removed persistent transaction should not reload.");

  clean(path);
}

void testPersistIsIdempotent() {
  const std::filesystem::path path = tempPath("idempotent");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction transaction = signedTransfer("b", 2);

  requireCondition(PersistentMempoolStore::persistTransaction(
                       directoryConfig, transaction,
                       transactionKeyPair("b").publicKey(), kTimestamp + 30)
                       .stored(),
                   "First persistent transaction write should store.");

  requireCondition(
      PersistentMempoolStore::persistTransaction(
          directoryConfig, transaction, transactionKeyPair("b").publicKey(),
          kTimestamp + 30)
          .alreadyStored(),
      "Second identical persistent transaction write should be idempotent.");

  clean(path);
}

void testLoadWithAccountStateAcceptsSufficientBalance() {
  const std::filesystem::path path = tempPath("sufficient-balance");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction transaction = signedTransfer("c", 1);

  requireCondition(
      PersistentMempoolStore::persistTransaction(
          directoryConfig, transaction, transactionKeyPair("c").publicKey(),
          kTimestamp + 40)
          .stored(),
      "Persistent transaction fixture should store before balance-aware load.");

  Mempool mempool;
  const Ed25519SignatureProvider provider;

  const auto loaded = PersistentMempoolStore::loadIntoMempool(
      directoryConfig, mempool, CryptoPolicy::developmentPolicy(),
      SecurityContext::USER_TRANSACTION, accountStateWithSenderBalance(1100),
      100, provider);

  requireCondition(loaded.loaded() && loaded.loadedTransactionCount() == 1U &&
                       mempool.size() == 1U,
                   "Balance-aware persistent mempool load should accept "
                   "sufficient balance.");

  clean(path);
}

void testLoadWithAccountStateRejectsInsufficientBalance() {
  const std::filesystem::path path = tempPath("insufficient-balance");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction transaction = signedTransfer("d", 1);

  requireCondition(PersistentMempoolStore::persistTransaction(
                       directoryConfig, transaction,
                       transactionKeyPair("d").publicKey(), kTimestamp + 50)
                       .stored(),
                   "Persistent transaction fixture should store before "
                   "insufficient-balance load.");

  Mempool mempool;
  const Ed25519SignatureProvider provider;

  const auto loaded = PersistentMempoolStore::loadIntoMempool(
      directoryConfig, mempool, CryptoPolicy::developmentPolicy(),
      SecurityContext::USER_TRANSACTION, accountStateWithSenderBalance(1099),
      100, provider);

  requireCondition(!loaded.loaded() &&
                       loaded.reason().find("balance is insufficient") !=
                           std::string::npos &&
                       mempool.empty(),
                   "Balance-aware persistent mempool load should reject "
                   "insufficient balance.");

  clean(path);
}

void testMalformedPersistentMempoolFileIsRejected() {
  const std::filesystem::path path = tempPath("malformed");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const std::filesystem::path malformedPath =
      directoryConfig.mempoolDirectoryPath() / "tx_malformed.nodo";

  {
    std::ofstream output(malformedPath);
    output << "NODO_MEMPOOL_TRANSACTION_V3\n"
           << "transactionId=abc\n"
           << "acceptedAt=" << (kTimestamp + 40) << "\n"
           << "transaction=Transaction{bad}\n"
           << "unknownField=must-fail\n";
  }

  Mempool mempool;

  const auto loaded = PersistentMempoolStore::loadIntoMempool(
      directoryConfig, mempool, CryptoPolicy::developmentPolicy(),
      SecurityContext::USER_TRANSACTION);

  requireCondition(!loaded.loaded() && loaded.reason().find("unknownField") !=
                                           std::string::npos,
                   "Malformed persistent mempool file should reject load.");

  clean(path);
}

void testCollectTransactionsPendingGossipSkipsAlreadyAdmitted() {
  const std::filesystem::path path = tempPath("collect-pending-gossip");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction pendingTx = signedTransfer("e", 1);
  const Transaction admittedTx = signedTransfer("e", 2);

  requireCondition(PersistentMempoolStore::persistTransaction(
                       directoryConfig, pendingTx,
                       transactionKeyPair("e").publicKey(), kTimestamp + 60)
                       .stored(),
                   "First fixture transaction should persist.");
  requireCondition(PersistentMempoolStore::persistTransaction(
                       directoryConfig, admittedTx,
                       transactionKeyPair("e").publicKey(), kTimestamp + 61)
                       .stored(),
                   "Second fixture transaction should persist.");

  Mempool mempool;
  requireCondition(
      mempool
          .admitTransaction(admittedTx, CryptoPolicy::developmentPolicy(),
                            SecurityContext::USER_TRANSACTION, kTimestamp + 62)
          .success(),
      "Fixture mempool should admit the already-known transaction.");

  const auto pending = PersistentMempoolStore::collectTransactionsPendingGossip(
      directoryConfig, mempool);

  requireCondition(pending.size() == 1U &&
                       pending.front().transaction.id() == pendingTx.id(),
                   "Only the not-yet-admitted transaction should be returned.");

  const auto decoded = PersistentMempoolStore::deserializeGossip(
      pending.front().gossipPayload, CryptoPolicy::developmentPolicy(),
      SecurityContext::USER_TRANSACTION, "nodo-localnet-1");

  requireCondition(
      decoded.has_value() && decoded->transaction.id() == pendingTx.id(),
      "Returned gossip payload should decode back to the pending transaction.");

  clean(path);
}

void testCollectTransactionsPendingGossipHandlesMissingDirectory() {
  const std::filesystem::path path = tempPath("collect-pending-gossip-missing");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  Mempool mempool;

  const auto pending = PersistentMempoolStore::collectTransactionsPendingGossip(
      directoryConfig, mempool);

  requireCondition(
      pending.empty(),
      "A data directory with no mempool folder should yield no pending "
      "transactions.");
}

void testCollectTransactionsPendingGossipSkipsMalformedFiles() {
  const std::filesystem::path path =
      tempPath("collect-pending-gossip-malformed");

  clean(path);

  const NodeDataDirectoryConfig directoryConfig(path);
  initDirectory(directoryConfig);

  const Transaction validTx = signedTransfer("f", 1);
  requireCondition(PersistentMempoolStore::persistTransaction(
                       directoryConfig, validTx,
                       transactionKeyPair("f").publicKey(), kTimestamp + 70)
                       .stored(),
                   "Valid fixture transaction should persist.");

  const std::filesystem::path malformedPath =
      directoryConfig.mempoolDirectoryPath() / "tx_malformed2.nodo";
  {
    std::ofstream output(malformedPath);
    output << "NODO_MEMPOOL_TRANSACTION_V3\n"
           << "transactionId=abc\n"
           << "acceptedAt=" << (kTimestamp + 40) << "\n"
           << "transaction=Transaction{bad}\n"
           << "unknownField=must-fail\n";
  }

  Mempool mempool;
  const auto pending = PersistentMempoolStore::collectTransactionsPendingGossip(
      directoryConfig, mempool);

  requireCondition(pending.size() == 1U &&
                       pending.front().transaction.id() == validTx.id(),
                   "A malformed file should be skipped, leaving only the valid "
                   "transaction.");

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
    testCollectTransactionsPendingGossipSkipsAlreadyAdmitted();
    testCollectTransactionsPendingGossipHandlesMissingDirectory();
    testCollectTransactionsPendingGossipSkipsMalformedFiles();

    std::cout << "Nodo persistent mempool store tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo persistent mempool store tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
