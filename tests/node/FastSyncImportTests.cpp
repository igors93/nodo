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

std::string validatorAddress() { return validatorKey().address().value(); }

void admitTransaction(NodeRuntime &runtime, const Transaction &tx,
                      std::int64_t timestamp) {
  auto result = runtime.mutableMempool().admitTransaction(
      tx, CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION,
      timestamp);
  require(result.accepted(),
          "Transaction must enter the mempool: " + result.reason());
}

void admit(NodeRuntime &runtime, std::uint64_t nonce, std::int64_t timestamp) {
  Transaction tx = TransactionBuilder::buildSignedTransfer(
      TransactionBuildRequest(
          "some-recipient", nodo::utils::Amount::fromRawUnits(1000),
          nodo::utils::Amount::fromRawUnits(100), nonce, timestamp),
      userSigner(), genesisConfig().networkParameters().chainId());
  admitTransaction(runtime, tx, timestamp);
}

void admitStakeDeposit(NodeRuntime &runtime, std::uint64_t nonce,
                       std::int64_t timestamp) {
  Transaction tx = TransactionBuilder::buildSignedStakeDeposit(
      TransactionBuildRequest(
          validatorAddress(), nodo::utils::Amount::fromRawUnits(5000000),
          nodo::utils::Amount::fromRawUnits(100), nonce, timestamp),
      userSigner(), genesisConfig().networkParameters().chainId());
  admitTransaction(runtime, tx, timestamp);
}

// A large votingPeriodBlocks keeps the proposal ACTIVE (rather than decided)
// through the snapshot below: the fast-sync fidelity check only needs
// non-trivial governance state to exist at snapshot time, not a completed
// decision.
std::string admitGovernanceProposal(NodeRuntime &runtime, std::uint64_t nonce,
                                    std::int64_t timestamp) {
  Transaction tx = TransactionBuilder::buildSignedGovernanceProposal(
      GovernanceProposalPayload::parameterChange(
          "Lower fee", "Reduce the minimum transaction fee",
          "MINIMUM_FEE_RAW", "250", /*effectiveHeight=*/500,
          /*votingStartDelayBlocks=*/0, /*votingPeriodBlocks=*/1000)
          .serialize(),
      nodo::utils::Amount::fromRawUnits(100), nonce, timestamp, userSigner(),
      genesisConfig().networkParameters().chainId());
  admitTransaction(runtime, tx, timestamp);
  return tx.id();
}

void admitGovernanceVote(NodeRuntime &runtime, const std::string &proposalId,
                         std::uint64_t nonce, std::int64_t timestamp) {
  const GovernanceVotePayload votePayload(proposalId, validatorAddress(),
                                          GovernanceVoteChoice::YES);
  Transaction tx = TransactionBuilder::buildSignedGovernanceVote(
      proposalId, votePayload, nodo::utils::Amount::fromRawUnits(100), nonce,
      timestamp, userSigner(), genesisConfig().networkParameters().chainId());
  admitTransaction(runtime, tx, timestamp);
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

    // 1. Setup Source Node. Block 1 carries a stake deposit and a governance
    // proposal, block 2 carries a vote on that proposal, so the snapshot
    // below captures non-trivial governance and staking state — the exact
    // state that the old ProtocolExecutionStateParser silently dropped
    // (governance) or only crudely approximated (staking) on fast-sync
    // import.
    NodeRuntime sourceNode = startRuntime();
    admitStakeDeposit(sourceNode, 1, kTimestamp + 1);
    const std::string proposalId =
        admitGovernanceProposal(sourceNode, 2, kTimestamp + 1);
    std::cout << "Producing block 1..." << std::endl;
    produceBlock(sourceNode, kTimestamp + 2, sourceConfig);
    std::cout << "Block 1 produced." << std::endl;
    admitGovernanceVote(sourceNode, proposalId, 3, kTimestamp + 3);
    std::cout << "Producing block 2..." << std::endl;
    produceBlock(sourceNode, kTimestamp + 4, sourceConfig);
    std::cout << "Block 2 produced." << std::endl;

    require(sourceNode.governanceExecutor().proposalStatus(proposalId) ==
                GovernanceProposalStatus::ACTIVE,
            "Fixture proposal must still be open for voting at snapshot "
            "time.");
    const GovernanceExecutor::GovernanceProposalSnapshot
        sourceProposalSnapshot =
            sourceNode.governanceExecutor().proposalSnapshot(proposalId);
    const nodo::utils::Amount sourceValidatorStake =
        sourceNode.stakingRegistry().activeStakeFor(validatorAddress());
    require(sourceValidatorStake.rawUnits() > 0,
            "Fixture stake deposit must be reflected before the snapshot is "
            "taken.");

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

    // 5b. Governance and staking fidelity: the fast-synced target must
    // reconstruct the exact same proposal/vote/stake state the source node
    // had at snapshot time, not silently drop it (governance) or crudely
    // approximate it (staking) as the old ProtocolExecutionStateParser did.
    const GovernanceExecutor::GovernanceProposalSnapshot
        targetProposalSnapshot =
            targetNode.governanceExecutor().proposalSnapshot(proposalId);
    require(targetProposalSnapshot.proposalId == sourceProposalSnapshot.proposalId &&
                targetProposalSnapshot.status == sourceProposalSnapshot.status &&
                targetProposalSnapshot.votingStartHeight ==
                    sourceProposalSnapshot.votingStartHeight &&
                targetProposalSnapshot.votingEndHeight ==
                    sourceProposalSnapshot.votingEndHeight &&
                targetProposalSnapshot.totalEligibleWeight ==
                    sourceProposalSnapshot.totalEligibleWeight,
            "Fast-synced governance proposal must match the source node's "
            "proposal exactly.");
    require(targetProposalSnapshot.votes.size() ==
                sourceProposalSnapshot.votes.size() &&
                !targetProposalSnapshot.votes.empty() &&
                targetProposalSnapshot.votes.front().validatorAddress ==
                    sourceProposalSnapshot.votes.front().validatorAddress &&
                targetProposalSnapshot.votes.front().choice ==
                    sourceProposalSnapshot.votes.front().choice &&
                targetProposalSnapshot.votes.front().weight ==
                    sourceProposalSnapshot.votes.front().weight &&
                targetProposalSnapshot.votes.front().castAt ==
                    sourceProposalSnapshot.votes.front().castAt,
            "Fast-synced governance vote must survive with full fidelity "
            "(the old parser dropped governance state entirely).");
    require(targetNode.stakingRegistry().activeStakeFor(validatorAddress()) ==
                sourceValidatorStake,
            "Fast-synced staking state must match the source node's stake "
            "exactly (the old parser only crudely re-derived it from "
            "validator stake amounts).");

    // 6. Produce Next Block on Source Node
    admit(sourceNode, 4, kTimestamp + 6);
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
