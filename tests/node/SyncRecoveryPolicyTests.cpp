#include "node/SyncRecoveryPolicy.hpp"

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/hash.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string root(char c) { return std::string(64, c); }

nodo::core::LedgerRecord record(const std::string &payload,
                                std::int64_t timestamp) {
  char payloadHashBuf[NODO_HASH_BUFFER_SIZE] = {};
  nodo_hash_string(payload.c_str(), payloadHashBuf, sizeof(payloadHashBuf));
  const std::string payloadHash(payloadHashBuf);
  const std::string sourceId = "sync-recovery-test-" + payloadHash.substr(0, 8);

  const std::string idInput =
      "LedgerRecordId{type=TRANSACTION;sourceId=" + sourceId +
      ";payloadHash=" + payloadHash +
      ";timestamp=" + std::to_string(timestamp) + "}";
  char idBuf[NODO_HASH_BUFFER_SIZE] = {};
  nodo_hash_string(idInput.c_str(), idBuf, sizeof(idBuf));

  return nodo::core::LedgerRecord::fromPersistedFields(
      std::string(idBuf), nodo::core::LedgerRecordType::TRANSACTION, sourceId,
      payload, payloadHash, timestamp);
}

nodo::core::Blockchain chainWithThreeBlocks() {
  using nodo::core::Block;
  nodo::core::Blockchain chain;
  const Block genesis =
      Block::createGenesisBlock({record("genesis", 100)}, 100, root('a'));
  chain.addGenesisBlock(genesis);
  const Block one(1, genesis.hash(), {record("one", 101)}, 101, root('b'),
                  root('c'));
  chain.addBlock(one);
  const Block two(2, one.hash(), {record("two", 102)}, 102, root('d'),
                  root('e'));
  chain.addBlock(two);
  return chain;
}

nodo::node::PersistentBlockSyncItem itemForBlock(const nodo::core::Block &block,
                                                 std::int64_t now) {
  return nodo::node::PersistentBlockSyncItem(
      block.index(), block.hash(), block.previousHash(), block.serialize(),
      block.hasCanonicalStateRoot() ? block.stateRoot() : root('f'), now,
      "serialized-finalized-record-placeholder");
}

void testServeRequestAccepted() {
  const auto chain = chainWithThreeBlocks();
  const nodo::node::NetworkBlockSyncRequest request(
      "node-b", nodo::node::BlockLocator(2, 16, {chain.blocks()[1].hash()}),
      1000);

  const auto decision = nodo::node::SyncRecoveryPolicy::evaluateServeRequest(
      request, "node-b", chain, 4);

  assert(decision.accepted());
  assert(decision.fromHeight() == 2);
  assert(decision.maxItems() == 1);
  assert(decision.expectedAncestorHash() == chain.blocks()[1].hash());
}

void testServeRequestRejectsWrongAncestor() {
  const auto chain = chainWithThreeBlocks();
  const nodo::node::NetworkBlockSyncRequest request(
      "node-b", nodo::node::BlockLocator(2, 16, {std::string(64, '0')}), 1000);

  const auto decision = nodo::node::SyncRecoveryPolicy::evaluateServeRequest(
      request, "node-b", chain, 4);

  assert(decision.rejected());
  assert(decision.reason().find("ancestor") != std::string::npos);
}

void testResponseBatchStaleWhenEverythingAlreadyImported() {
  const auto chain = chainWithThreeBlocks();
  const nodo::node::PersistentSyncCheckpoint checkpoint(
      nodo::node::PersistentSyncCheckpoint::SCHEMA_VERSION, 2,
      chain.latestBlock().hash(), chain.latestBlock().stateRoot(),
      nodo::node::PersistentSyncStatus::COMPLETE, "node-a", 1000);

  const nodo::node::PersistentBlockSyncBatch batch(
      "node-a", 1, 1, {itemForBlock(chain.blocks()[1], 1000)}, 1000);

  const auto decision = nodo::node::SyncRecoveryPolicy::evaluateResponseBatch(
      batch, "node-a", checkpoint, chain);

  assert(decision.stale());
}

void testResponseBatchRejectsForkedOverlap() {
  auto chain = chainWithThreeBlocks();
  const nodo::core::Block conflicting(1, chain.blocks()[0].hash(),
                                      {record("conflicting", 101)}, 101,
                                      root('b'), root('c'));
  const nodo::node::PersistentSyncCheckpoint checkpoint(
      nodo::node::PersistentSyncCheckpoint::SCHEMA_VERSION, 2,
      chain.latestBlock().hash(), chain.latestBlock().stateRoot(),
      nodo::node::PersistentSyncStatus::COMPLETE, "node-a", 1000);

  const nodo::node::PersistentBlockSyncBatch batch(
      "node-a", 1, 1, {itemForBlock(conflicting, 1000)}, 1000);

  const auto decision = nodo::node::SyncRecoveryPolicy::evaluateResponseBatch(
      batch, "node-a", checkpoint, chain);

  assert(decision.rejected());
  assert(decision.reason().find("conflicts") != std::string::npos);
}

void testTargetRequiresMatchingHash() {
  const auto chain = chainWithThreeBlocks();
  assert(nodo::node::SyncRecoveryPolicy::targetReached(
      chain, 2, chain.blocks()[2].hash()));
  assert(!nodo::node::SyncRecoveryPolicy::targetReached(chain, 2,
                                                        std::string(64, '0')));
}

} // namespace

int main() {
  testServeRequestAccepted();
  testServeRequestRejectsWrongAncestor();
  testResponseBatchStaleWhenEverythingAlreadyImported();
  testResponseBatchRejectsForkedOverlap();
  testTargetRequiresMatchingHash();
  std::cout << "Nodo SyncRecoveryPolicy tests passed.\n";
  return 0;
}
