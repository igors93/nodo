#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "node/ParallelBlockSync.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::LedgerRecord;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::node::ParallelBlockSync;
using nodo::node::SyncCheckpoint;
using nodo::node::SyncPhase;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

ValidationWorkRecord makeWork(const std::string &id) {
  return ValidationWorkRecord("parallel-sync-validator", 1,
                              ValidationWorkType::VALIDATE_BLOCK,
                              ValidationWorkResult::ACCEPTED,
                              "parallel-sync-target-" + id, id, 1, kTimestamp);
}

Block makeBlock(std::uint64_t index, const std::string &prevHash,
                const std::string &suffix) {
  return Block(index, prevHash,
               {LedgerRecord::fromValidationWorkRecord(
                   makeWork("sync-" + suffix),
                   kTimestamp + static_cast<std::int64_t>(index))},
               kTimestamp + static_cast<std::int64_t>(index));
}

void testBeginSyncReturnsDownloadingHeaders() {
  ParallelBlockSync sync;
  const auto progress = sync.beginSync(0, 10, kTimestamp);

  requireCondition(progress.phase == SyncPhase::DOWNLOADING_HEADERS,
                   "beginSync should return DOWNLOADING_HEADERS phase.");

  requireCondition(progress.localHeight == 0 && progress.targetHeight == 10,
                   "beginSync should set correct local/target heights.");
}

void testOnHeadersReceivedAdvancesPhase() {
  ParallelBlockSync sync;
  sync.beginSync(0, 2, kTimestamp);

  // Build a genesis block to use as header
  Block genesis = Block::createGenesisBlock(
      {LedgerRecord::fromValidationWorkRecord(makeWork("gen"), kTimestamp)},
      kTimestamp);

  const std::vector<std::string> headers = {genesis.serialize()};

  const bool accepted = sync.onHeadersReceived(headers, 0, "peer-1");

  requireCondition(accepted, "onHeadersReceived should accept valid headers.");

  const auto progress = sync.progress();
  requireCondition(
      progress.headersDownloaded >= 1,
      "headersDownloaded should be incremented after onHeadersReceived.");
}

void testMatchesCheckpointValidatesKnownCheckpoints() {
  SyncCheckpoint cp;
  cp.height = 100;
  cp.blockHash = "known-hash-abc";
  cp.timestamp = kTimestamp;

  ParallelBlockSync sync({cp});
  sync.beginSync(0, 200, kTimestamp);

  requireCondition(
      sync.matchesCheckpoint(100, "known-hash-abc"),
      "matchesCheckpoint should return true for correct hash at known height.");

  requireCondition(
      !sync.matchesCheckpoint(100, "wrong-hash-xyz"),
      "matchesCheckpoint should return false for wrong hash at known height.");

  requireCondition(
      sync.matchesCheckpoint(50, "any-hash-here"),
      "matchesCheckpoint should return true at height without a checkpoint.");
}

void testOnBodyReceivedOnlyAcceptsVerifiedHeaders() {
  ParallelBlockSync sync;
  sync.beginSync(0, 5, kTimestamp);

  // Attempt to insert a body without a verified header
  Block genesis =
      Block::createGenesisBlock({LedgerRecord::fromValidationWorkRecord(
                                    makeWork("gen-body"), kTimestamp)},
                                kTimestamp);

  const bool rejected = sync.onBodyReceived(genesis.serialize(), 0, "peer-1");
  requireCondition(!rejected, "onBodyReceived should reject body when no "
                              "verified header exists for that height.");

  // Now add the header first
  const bool headerAccepted =
      sync.onHeadersReceived({genesis.serialize()}, 0, "peer-1");
  requireCondition(headerAccepted, "Header should be accepted.");

  // Now the body should be accepted
  const bool bodyAccepted =
      sync.onBodyReceived(genesis.serialize(), 0, "peer-1");
  requireCondition(
      bodyAccepted,
      "onBodyReceived should accept body when verified header exists.");
}

void testDrainReadyBlocksReturnsOrderedBlocks() {
  ParallelBlockSync sync;
  sync.beginSync(0, 2, kTimestamp);

  // Build two consecutive blocks
  Block block0 = Block::createGenesisBlock(
      {LedgerRecord::fromValidationWorkRecord(makeWork("drain-0"), kTimestamp)},
      kTimestamp);

  Block block1(1, block0.hash(),
               {LedgerRecord::fromValidationWorkRecord(makeWork("drain-1"),
                                                       kTimestamp + 1)},
               kTimestamp + 1);

  // Feed headers for both
  sync.onHeadersReceived({block0.serialize()}, 0, "peer-1");
  sync.onHeadersReceived({block1.serialize()}, 1, "peer-1");

  // Feed bodies for both
  sync.onBodyReceived(block0.serialize(), 0, "peer-1");
  sync.onBodyReceived(block1.serialize(), 1, "peer-1");

  const auto ready = sync.drainReadyBlocks();

  requireCondition(ready.size() == 2, "drainReadyBlocks should return 2 blocks "
                                      "when both header+body are present.");

  // Verify order: first block should be height 0
  const auto first = nodo::core::Block::deserialize(ready[0]);
  requireCondition(first.has_value() && first.value().index() == 0,
                   "First drained block should be at height 0.");

  const auto second = nodo::core::Block::deserialize(ready[1]);
  requireCondition(second.has_value() && second.value().index() == 1,
                   "Second drained block should be at height 1.");
}

} // namespace

int main() {
  try {
    testBeginSyncReturnsDownloadingHeaders();
    testOnHeadersReceivedAdvancesPhase();
    testMatchesCheckpointValidatesKnownCheckpoints();
    testOnBodyReceivedOnlyAcceptsVerifiedHeaders();
    testDrainReadyBlocksReturnsOrderedBlocks();

    std::cout << "Nodo ParallelBlockSync tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo ParallelBlockSync tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
