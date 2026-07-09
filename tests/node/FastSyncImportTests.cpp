#include "config/GenesisRegistry.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "core/Block.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/FastSyncSnapshotService.hpp"
#include "node/FastSyncSnapshotStore.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/RuntimeBlockPipeline.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nodo::node;
using namespace nodo::core;
using namespace nodo::config;
using namespace nodo::crypto;
using namespace nodo::p2p;

constexpr std::int64_t kTimestamp = 1700000000LL;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

KeyPair validatorKey() {
  return KeyPair::createDeterministicBls12381KeyPair("fastsync-validator");
}

KeyPair userKey() {
  return KeyPair::createDeterministicEd25519KeyPair("fastsync-user");
}

Signer validatorSigner() {
  static const Bls12381SignatureProvider provider;
  return Signer(validatorKey(), provider);
}

Signer userSigner() {
  static const Ed25519SignatureProvider provider;
  return Signer(userKey(), provider);
}

GenesisConfig genesisConfig() {
  return GenesisConfig(
      NetworkParameters::developmentLocal(), kTimestamp,
      {BootstrapValidatorConfig(validatorKey().publicKey(), 1, 1,
                                "fastsync-validator",
                                userKey().address().value())},
      {GenesisAccountConfig(userKey().address().value(),
                            nodo::utils::Amount::fromRawUnits(1000000000000),
                            0)},
      "fastsync-genesis");
}

void admit(NodeRuntime &runtime, std::uint64_t nonce, std::int64_t timestamp) {
  Transaction tx = TransactionBuilder::buildSignedTransfer(
      TransactionBuildRequest(
          "some-recipient", nodo::utils::Amount::fromRawUnits(1000),
          nodo::utils::Amount::fromRawUnits(100), nonce, timestamp),
      userSigner(), genesisConfig().networkParameters().chainId());
  auto result = runtime.mutableMempool().admitTransaction(
      tx, CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION,
      timestamp);
  require(result.accepted(),
          "Transaction must enter the mempool: " + result.reason());
}

NodeRuntime startRuntime() {
  const auto started = NodeRuntimeFactory::startFromGenesis(NodeRuntimeConfig(
      genesisConfig(),
      PeerInfo("local", "127.0.0.1:29996", "nodo/test", 0, kTimestamp), 16));
  require(started.started(), "Runtime must start from genesis.");
  return started.runtime();
}

void produceBlock(NodeRuntime &runtime, std::int64_t timestamp,
                  const NodeDataDirectoryConfig &directoryConfig) {
  const auto result = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
      runtime, RuntimeBlockPipelineConfig(10, 1, 1, timestamp),
      validatorSigner(), &directoryConfig);
  require(result.finalized(), "Block must finalize: " + result.reason());
}

int main() {
  try {
    std::filesystem::path dirSource =
        std::filesystem::temp_directory_path() / "nodo-fast-sync-source";
    std::filesystem::path dirTarget =
        std::filesystem::temp_directory_path() / "nodo-fast-sync-target";
    std::filesystem::remove_all(dirSource);
    std::filesystem::remove_all(dirTarget);

    NodeDataDirectoryConfig sourceConfig(dirSource);
    NodeDataDirectoryConfig targetConfig(dirTarget);
    auto sourceInit = NodeDataDirectory::initialize(
        sourceConfig, genesisConfig(),
        PeerInfo("local_source", "127.0.0.1:29996", "nodo/test", 0, kTimestamp),
        kTimestamp);
    require(sourceInit.success(),
            "Source node directory init failed: " + sourceInit.reason());

    auto targetInit = NodeDataDirectory::initialize(
        targetConfig, genesisConfig(),
        PeerInfo("local_target", "127.0.0.1:29997", "nodo/test", 0, kTimestamp),
        kTimestamp);
    require(targetInit.success(),
            "Target node directory init failed: " + targetInit.reason());

    // 1. Setup Source Node
    NodeRuntime sourceNode = startRuntime();
    admit(sourceNode, 1, kTimestamp + 1);
    std::cout << "Producing block 1..." << std::endl;
    produceBlock(sourceNode, kTimestamp + 2, sourceConfig);
    std::cout << "Block 1 produced." << std::endl;
    admit(sourceNode, 2, kTimestamp + 3);
    std::cout << "Producing block 2..." << std::endl;
    produceBlock(sourceNode, kTimestamp + 4, sourceConfig);
    std::cout << "Block 2 produced." << std::endl;

    // 2. Export Snapshot from Source Node
    FastSyncSnapshot snapshot =
        FastSyncSnapshotService::buildSnapshot(sourceNode, kTimestamp + 4);
    require(snapshot.isValid(), "Snapshot must be valid");
    require(snapshot.blockHeight() == 2, "Snapshot height should be 2");

    FastSyncSnapshotStore store(targetConfig.runtimeDirectoryPath() /
                                "fast_sync_snapshots");
    require(store.save(snapshot), "Failed to save snapshot to target store");

    PersistentSnapshotSyncManifest manifest(
        "source-peer", snapshot.blockHeight(), snapshot.blockHash(),
        snapshot.stateRoot(), snapshot.digest(), snapshot.createdAt());

    // 3. Setup Empty Target Node
    NodeRuntime targetNode = startRuntime();
    require(targetNode.blockchain().latestBlock().index() == 0,
            "Target node must start at genesis");

    PersistentSyncCheckpoint localCheckpoint(
        PersistentSyncCheckpoint::SCHEMA_VERSION, 0,
        targetNode.blockchain().latestBlock().hash(), std::string(64, '0'),
        PersistentSyncStatus::SYNCING, "local", kTimestamp);

    // 4. Planner Test
    ChainStatusMessage remoteStatus(
        "localnet", genesisConfig().networkParameters().chainId(), "1.0", 2,
        snapshot.blockHash(), 2, snapshot.blockHash());
    PersistentSyncPlan plan =
        PersistentBlockStateSyncPlanner::planFromRemoteStatus(
            localCheckpoint, remoteStatus, "target-peer", "source-peer", 100,
            kTimestamp + 2);
    require(plan.status() == PersistentSyncPlanStatus::REQUEST_BLOCKS,
            "Planner should request blocks when gap is small");

    // 5. Positive Test: Import Snapshot
    auto result = PersistentBlockStateSyncApplier::importSnapshot(
        localCheckpoint, manifest, targetNode, targetConfig, nullptr,
        kTimestamp + 5);
    require(result.applied(),
            "Valid snapshot must be imported successfully: " + result.reason());
    require(targetNode.blockchain().latestBlock().index() == 2,
            "Target node should jump to height 2");

    // 6. Produce Next Block on Source Node
    admit(sourceNode, 3, kTimestamp + 6);
    produceBlock(sourceNode, kTimestamp + 7, sourceConfig);
    const Block nextBlock = sourceNode.blockchain().latestBlock();
    require(nextBlock.index() == 3, "Source node should be at height 3");

    // 7. Sync Next Block on Target Node
    const nodo::consensus::FinalizedBlockRecord *sourceRecord =
        sourceNode.finalizationRegistry().recordForHeight(nextBlock.index());
    require(sourceRecord != nullptr,
            "Source node must have finalized the next block");

    PersistentBlockSyncItem item(nextBlock.index(), nextBlock.hash(),
                                 nextBlock.previousHash(),
                                 nextBlock.serialize(), nextBlock.stateRoot(),
                                 kTimestamp + 7, sourceRecord->serialize());
    PersistentBlockSyncBatch batch("source-peer", 3, 3, {item}, kTimestamp + 7);

    PersistentSyncCheckpoint updatedCheckpoint = *result.checkpoint();
    std::cout << "isRunning: " << targetNode.isRunning() << "\n";
    std::cout << "config.isValid: " << targetNode.config().isValid() << "\n";
    std::cout << "blockchain.empty: " << targetNode.blockchain().empty()
              << "\n";
    std::cout << "blockchain.isValid: "
              << targetNode.blockchain().isValid(false) << "\n";
    std::cout << "validatorRegistry.isValid: "
              << targetNode.validatorRegistry().isValid() << "\n";
    std::cout << "validatorRegistry.activeCount: "
              << targetNode.validatorRegistry().activeCount() << "\n";
    std::cout << "validatorRegistry.totalConsensusWeight: "
              << targetNode.validatorRegistry().totalConsensusWeight() << "\n";
    std::cout << "validatorPenaltyLedger.isValid: "
              << targetNode.validatorPenaltyLedger().isValid() << "\n";
    std::cout << "validatorSetHistory.isValid: "
              << targetNode.validatorSetHistory().isValid() << "\n";
    std::cout << "hasSet: "
              << targetNode.validatorSetHistory().hasSet(
                     targetNode.consensusRoundManager().currentState().height())
              << "\n";
    std::cout << "consensusRoundManager.currentState.height: "
              << targetNode.consensusRoundManager().currentState().height()
              << "\n";
    std::cout << "finalizationRegistry.isValid: "
              << targetNode.finalizationRegistry().isValid() << "\n";
    std::cout << "consensusRoundManager.currentState().isValid: "
              << targetNode.consensusRoundManager().currentState().isValid()
              << "\n";
    std::cout << "consensusRoundManager.currentState: "
              << targetNode.consensusRoundManager().currentState().serialize()
              << "\n";
    std::cout << "peerManager.isValid: " << targetNode.peerManager().isValid()
              << "\n";

    auto blockResult = PersistentBlockStateSyncApplier::importFinalizedBatch(
        updatedCheckpoint, batch, targetNode, targetConfig, nullptr,
        kTimestamp + 8);
    require(blockResult.applied(), "Target node should successfully apply the "
                                   "next block after snapshot: " +
                                       blockResult.reason());
    require(targetNode.blockchain().latestBlock().index() == 3,
            "Target node should reach height 3");

    // 8. Negative Test: Invalid Manifest Hash
    PersistentSnapshotSyncManifest invalidManifest(
        "source-peer", snapshot.blockHeight(), snapshot.blockHash(),
        snapshot.stateRoot(), "invalid_digest", snapshot.createdAt());
    auto negativeResult = PersistentBlockStateSyncApplier::importSnapshot(
        localCheckpoint, invalidManifest, targetNode, targetConfig, nullptr,
        kTimestamp + 9);
    require(!negativeResult.applied(),
            "Snapshot import should fail with corrupted manifest hash");

    std::filesystem::remove_all(dirSource);
    std::filesystem::remove_all(dirTarget);

    std::cout << "Fast sync import tests passed successfully!\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << "\n";
    return 1;
  }
}
