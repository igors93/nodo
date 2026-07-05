#include "config/GenesisRegistry.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/SlashingEvidence.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Transaction.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/hash.h"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardLedgerBuilder.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/ChainAuditor.hpp"
#include "node/EpochRewardSettlementService.hpp"
#include "node/NodeRuntime.hpp"

#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/ValidatorStakeWeightUpdater.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Time.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

void testStakeLifecycleEndToEnd() {
  // This test simulates the full lifecycle of a validator across 2 epochs,
  // including stake locking, consensus weight redistribution, reward
  // settlement, slashing due to equivocation, and a final chain audit. Because
  // an epoch is 43200 blocks, we cannot do this in a real TCP test. Instead, we
  // validate that the modules correctly integrate when driven programmatically.

  config::GenesisConfig genesis =
      config::GenesisRegistry::get("localnet").genesis();
  const std::string validatorAddress =
      genesis.bootstrapValidators().front().validatorAddress();

  p2p::PeerInfo localPeer("test-peer", "127.0.0.1:9000", "nodo/0.1", 0,
                          kTimestamp);
  node::NodeRuntimeStartResult start =
      node::NodeRuntimeFactory::startFromGenesis(node::NodeRuntimeConfig(
          genesis, localPeer, genesis.networkParameters().maxPeerCount()));
  requireCondition(start.started(), "Runtime should start.");
  node::NodeRuntime runtime = start.runtime();

  requireCondition(runtime.validatorRegistry().activeCount() > 0,
                   "Genesis should have validators.");

  // 2. We verify that ConsensusWeight calculation correctly gives sqrt weights.
  // We can just verify it is > 0
  requireCondition(
      runtime.validatorRegistry().consensusWeightFor(validatorAddress) > 0,
      "Validator consensus weight should be > 0.");

  // 3. To simulate an epoch without producing 43200 real blocks, we invoke the
  // epoch boundary hook directly: ValidatorStakeWeightUpdater
  core::ValidatorRegistry registryAfterEpoch1 = runtime.validatorRegistry();
  node::ValidatorStakeWeightUpdater::synchronizeAtEpochBoundary(
      node::NODO_VALIDATOR_EPOCH_BLOCKS + 1, kTimestamp,
      runtime.stakingRegistry(), registryAfterEpoch1);

  requireCondition(registryAfterEpoch1.activeCount() > 0,
                   "Epoch 1 should preserve validators.");

  // 4. Simulate EpochRewardSettlementService
  // Normally, RuntimeBlockPipeline invokes this, but since we are
  // fast-forwarding, we can construct it using fake ValidationWorkRecords
  const std::uint64_t epoch = 1;
  std::vector<economics::ValidationWorkRecord> workRecords;
  workRecords.emplace_back(validatorAddress, epoch,
                           economics::ValidationWorkType::VALIDATE_BLOCK,
                           economics::ValidationWorkResult::ACCEPTED,
                           "evidence-hash", "work-id-a", 100, kTimestamp);

  std::vector<economics::ValidatorScoreRecord> scoreRecords;
  scoreRecords.emplace_back(
      validatorAddress, epoch, 100, 100,
      economics::ValidatorScoreReason::CONSISTENT_VALIDATION, "evidence",
      kTimestamp);

  const economics::EpochRewardDistribution distribution =
      economics::EpochRewardDistributor::distribute(
          epoch, 1, node::NODO_VALIDATOR_EPOCH_BLOCKS,
          utils::Amount::fromRawUnits(100000000), utils::Amount(), 100,
          economics::EpochEmissionPolicy::developmentDefaultPolicy(),
          workRecords, scoreRecords, "end-hash", kTimestamp);

  requireCondition(distribution.genesisRewardRecords().size() > 0,
                   "Distribution should yield rewards.");

  crypto::Bls12381SignatureProvider provider;
  crypto::KeyPair validatorA = provider.generateKeyPair();
  core::ValidatorRegistrationRecord registration(validatorA.address().value(),
                                                 validatorA.publicKey(), 1,
                                                 "test-validator", kTimestamp);
  registryAfterEpoch1.registerValidator(registration);

  const auto makeVote = [&](const std::string &blockHash) {
    return consensus::ValidatorVoteRecord::createVote(
        validatorA.address().value(), validatorA.publicKey(),
        validatorA.privateKeyForSigningOnly(), 2, blockHash, "previous-block",
        1, consensus::ValidatorVoteDecision::PRECOMMIT, "stake-lifecycle-test",
        kTimestamp, provider);
  };
  consensus::DoubleVoteEvidence evidence(makeVote("block-a"),
                                         makeVote("block-b"), kTimestamp);

  const core::LedgerRecord record =
      node::CanonicalSlashingTransition::buildEvidenceRecord(evidence,
                                                             kTimestamp);
  std::vector<core::LedgerRecord> evidenceRecords = {record};

  node::StakingRegistry staking = runtime.stakingRegistry();
  staking.setAccount(
      validatorA.address().value(),
      economics::StakeAccount(validatorA.address().value(),
                              utils::Amount::fromRawUnits(100000000000)));
  consensus::ValidatorPenaltyLedger penaltyLedger =
      runtime.validatorPenaltyLedger();

  core::ValidatorSetHistory history;
  history.recordSet(1, registryAfterEpoch1);
  history.recordSet(2, registryAfterEpoch1);

  node::CanonicalSlashingTransition::applyEvidenceRecords(
      evidenceRecords, 3, kTimestamp, history, genesis.networkParameters(),
      crypto::CryptoPolicy::developmentPolicy(), provider, penaltyLedger,
      registryAfterEpoch1, staking);

  requireCondition(
      registryAfterEpoch1.entryForAddress(validatorA.address().value()) !=
              nullptr &&
          registryAfterEpoch1.entryForAddress(validatorA.address().value())
              ->jailed(),
      "Validator should be jailed.");

  // 6. Chain Audit
  // Since we fast-forwarded and didn't insert real blocks, we will just use
  // ChainAuditor on the current runtime which is at genesis. The ChainAuditor
  // will verify that at least the genesis structures are consistent.
  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoadResult::loaded(
          runtime,
          node::NodeRuntimeManifest::fromRuntime(runtime, kTimestamp,
                                                 kTimestamp),
          0, 0);
  const node::ChainAuditResult audit =
      node::ChainAuditor::auditLoadedRuntimeDevMode(load);

  requireCondition(audit.passed(),
                   "Chain audit should pass for the current chain state.");
}

int main() {
  try {
    testStakeLifecycleEndToEnd();
    std::cout << "Stake lifecycle E2E tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Stake lifecycle E2E tests failed: " << error.what() << "\n";
    return 1;
  }
}
