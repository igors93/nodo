#include "../common/ConsensusPhaseTestFixtures.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900500000;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

const crypto::CryptoPolicy &developmentPolicy() {
  static const crypto::CryptoPolicy policy =
      crypto::CryptoPolicy::developmentPolicy();
  return policy;
}

crypto::KeyPair userKey() {
  return test::consensusTestUserKey("consensus-event-loop-user");
}

struct ValidatorFixture {
  crypto::KeyPair keyPair;
};

std::vector<ValidatorFixture> makeValidators(std::size_t count) {
  std::vector<ValidatorFixture> validators;
  validators.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    validators.push_back({crypto::KeyPair::createDeterministicBls12381KeyPair(
        "consensus-event-loop-validator-" + std::to_string(index))});
  }
  return validators;
}

config::GenesisConfig
makeGenesis(const std::vector<ValidatorFixture> &validators,
            const std::string &seed) {
  std::vector<config::BootstrapValidatorConfig> bootstrap;
  bootstrap.reserve(validators.size());
  for (std::size_t index = 0; index < validators.size(); ++index) {
    bootstrap.emplace_back(validators[index].keyPair.publicKey(), 1, 1,
                           "candidate-validator-" + std::to_string(index));
  }

  return config::GenesisConfig(
      config::NetworkParameters::developmentLocal(), kTimestamp, bootstrap,
      {test::fundedConsensusTestAccount(userKey())}, seed);
}

node::NodeRuntime startRuntime(const config::GenesisConfig &genesis) {
  const p2p::PeerInfo peer("candidate-test-node", "127.0.0.1:29995",
                           "nodo/test", 0, kTimestamp);
  const auto result = node::NodeRuntimeFactory::startFromGenesis(
      node::NodeRuntimeConfig(genesis, peer, 16));
  if (!result.started()) {
    throw std::runtime_error("Runtime start failed: " + result.reason());
  }
  return result.runtime();
}

p2p::PeerMetadata peerMetadata(const std::string &nodeId, std::uint16_t port) {
  return p2p::PeerMetadata(nodeId, p2p::PeerEndpoint("127.0.0.1", port),
                           nodeId + "-identity", kTimestamp, kTimestamp, 0,
                           false);
}

class TestNetwork {
private:
  std::shared_ptr<p2p::LoopbackTransportBus> m_bus;
  p2p::LoopbackTransport m_transport;

public:
  explicit TestNetwork(const config::GenesisConfig &genesis)
      : m_bus(std::make_shared<p2p::LoopbackTransportBus>()),
        m_transport(m_bus),
        mesh(p2p::GossipMeshConfig("candidate-test-node",
                                   genesis.networkParameters().networkName(),
                                   genesis.networkParameters().chainId(),
                                   "nodo/test", genesis.deterministicId(), 60,
                                   3),
             m_transport) {}

  p2p::GossipMesh mesh;
  p2p::GossipInbox validatedInbox;

  void routeMessages() {
    for (const auto &msg : mesh.drainAllInbox()) {
      validatedInbox.add(msg);
    }
  }
};

class TwoPeerConsensusNetwork {
private:
  std::shared_ptr<p2p::LoopbackTransportBus> m_bus;
  p2p::LoopbackTransport m_consensusTransport;
  p2p::LoopbackTransport m_peerTransport;

public:
  explicit TwoPeerConsensusNetwork(const config::GenesisConfig &genesis)
      : m_bus(std::make_shared<p2p::LoopbackTransportBus>()),
        m_consensusTransport(m_bus), m_peerTransport(m_bus),
        consensusMesh(p2p::GossipMeshConfig(
                          "candidate-test-node",
                          genesis.networkParameters().networkName(),
                          genesis.networkParameters().chainId(), "nodo/test",
                          genesis.deterministicId(), 60, 3),
                      m_consensusTransport),
        peerMesh(p2p::GossipMeshConfig(
                     "vote-conflict-peer",
                     genesis.networkParameters().networkName(),
                     genesis.networkParameters().chainId(), "nodo/test",
                     genesis.deterministicId(), 60, 3),
                 m_peerTransport) {
    require(
        consensusMesh.registerPeer(peerMetadata("vote-conflict-peer", 31902))
            .success(),
        "Consensus mesh must register the remote peer.");
    require(peerMesh.registerPeer(peerMetadata("candidate-test-node", 31901))
                .success(),
            "Remote peer must register the consensus mesh.");
    require(consensusMesh.connectPeer("vote-conflict-peer").success(),
            "Consensus mesh must connect to the remote peer.");
    require(peerMesh.connectPeer("candidate-test-node").success(),
            "Remote peer must connect to the consensus mesh.");
  }

  p2p::GossipMesh consensusMesh;
  p2p::GossipMesh peerMesh;
  p2p::GossipInbox validatedInbox;

  void routeMessages() {
    for (const auto &msg : consensusMesh.drainAllInbox()) {
      validatedInbox.add(msg);
    }
  }
};

const ValidatorFixture &
proposerFixture(const std::vector<ValidatorFixture> &validators,
                const node::NodeRuntime &runtime) {
  const auto &state = runtime.consensusRoundManager().currentState();
  const std::string expected = consensus::ProposerSchedule::selectProposer(
      runtime.validatorRegistry(),
      runtime.config().genesisConfig().networkParameters().chainId(),
      state.height(), state.round());

  for (const auto &validator : validators) {
    if (validator.keyPair.address().value() == expected)
      return validator;
  }
  throw std::runtime_error("Scheduled proposer key was not found.");
}

#include "consensus/BlockProposalPhase.hpp"

void injectProposal(node::NodeRuntime &runtime, p2p::GossipMesh &mesh,
                    const crypto::Signer &signer, std::int64_t now) {
  const std::uint64_t round = 1;
  const consensus::BlockCandidateResult candidate =
      consensus::BlockProductionPhase::produce(
          runtime, node::RuntimeBlockPipelineConfig(16, 1, round, now));
  require(candidate.produced(),
          "Candidate block must be produced for test injection.");

  static const crypto::Bls12381SignatureProvider provider;
  const consensus::BlockProposalResult proposal =
      consensus::BlockProposalPhase::propose(candidate.block(),
                                             signer.address(), round, now,
                                             signer, mesh, provider);
  require(proposal.proposed(), "Block proposal must be successful.");

  mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                          proposal.serializedProposal(), now);
}

void configureProducer(consensus::ConsensusEventLoop &loop,
                       const crypto::Signer &signer) {
  loop.setLocalValidatorAddress(signer.address());
  loop.setLocalSigner(&signer);
}

void sendVoteToConsensusPeer(TwoPeerConsensusNetwork &network,
                             const consensus::ValidatorVoteRecord &vote,
                             std::int64_t now) {
  require(network.peerMesh
                  .broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE,
                             vote.serialize(), now)
                  .acceptedCount() == 1,
          "Remote peer must enqueue validator vote for the consensus mesh.");
  require(network.peerMesh.flushOutbound(now).acceptedCount() == 1,
          "Remote peer must flush outbound validator vote.");
  require(network.consensusMesh.receiveAvailable(now).acceptedCount() == 1,
          "Consensus mesh must receive validator vote.");
}

consensus::ValidatorVoteRecord
makeSignedConsensusVote(const ValidatorFixture &validator,
                        const std::string &blockHash, std::int64_t createdAt,
                        const crypto::Bls12381SignatureProvider &provider) {
  return consensus::ValidatorVoteRecord::createVote(
      validator.keyPair.address().value(), validator.keyPair.publicKey(),
      validator.keyPair.privateKeyForSigningOnly(), 1, blockHash,
      "candidate-previous-hash", 1, consensus::ValidatorVoteDecision::PRECOMMIT,
      "reason-hash", createdAt, provider);
}

void testConflictingVoteCreatesEvidenceImmediately() {
  const auto validators = makeValidators(1);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "candidate-immediate-conflict-evidence");
  node::NodeRuntime runtime = startRuntime(genesis);
  TwoPeerConsensusNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  consensus::EvidencePool evidencePool;

  consensus::ConsensusEventLoop loop(runtime, network.consensusMesh,
                                     network.validatedInbox,
                                     developmentPolicy(), provider);
  loop.setEvidencePool(&evidencePool);

  const consensus::ValidatorVoteRecord first = makeSignedConsensusVote(
      validators.front(), "conflict-block-hash-a", kTimestamp + 1, provider);
  const consensus::ValidatorVoteRecord second = makeSignedConsensusVote(
      validators.front(), "conflict-block-hash-b", kTimestamp + 2, provider);

  sendVoteToConsensusPeer(network, first, kTimestamp + 3);
  sendVoteToConsensusPeer(network, second, kTimestamp + 4);

  network.routeMessages();
  const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 5);

  require(result.votesCollected == 1,
          "Only the first vote should be admitted to the vote pool.");
  require(result.evidenceAccepted == 1,
          "The conflicting vote must create slashing evidence during the same "
          "tick.");
  require(evidencePool.size() == 1,
          "Evidence pool must contain the immediate double-vote evidence.");

  const std::vector<consensus::DoubleVoteEvidence> stored =
      evidencePool.allDoubleVoteEvidence();
  require(
      stored.size() == 1 &&
          ((stored.front().firstVote().blockHash() == "conflict-block-hash-a" &&
            stored.front().secondVote().blockHash() ==
                "conflict-block-hash-b") ||
           (stored.front().firstVote().blockHash() == "conflict-block-hash-b" &&
            stored.front().secondVote().blockHash() ==
                "conflict-block-hash-a")) &&
          stored.front().detectedAt() == kTimestamp + 5,
      "Stored evidence must bind the original vote, conflicting vote and "
      "detection time.");

  network.consensusMesh.flushOutbound(kTimestamp + 6);
  network.peerMesh.receiveAvailable(kTimestamp + 6);
  const auto evidenceMessages = network.peerMesh.drainInbox(
      p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE);
  require(evidenceMessages.size() == 1,
          "Accepted conflict evidence must be gossiped immediately.");
  const node::SlashingEvidenceAnnouncement announcement =
      node::SlashingEvidenceAnnouncement::deserialize(
          evidenceMessages.front().payload());
  require(announcement.evidence().evidenceId() == stored.front().evidenceId(),
          "Gossiped evidence must match the stored double-vote evidence.");
}

void testCandidateDoesNotEnterChainWithoutQuorum() {
  const auto validators = makeValidators(2);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "candidate-no-quorum");
  node::NodeRuntime runtime = startRuntime(genesis);
  test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(proposerFixture(validators, runtime).keyPair,
                              provider);

  consensus::ConsensusEventLoop loop(runtime, network.mesh,
                                     network.validatedInbox, developmentPolicy(),
                                     provider);
  configureProducer(loop, signer);
  injectProposal(runtime, network.mesh, signer, kTimestamp + 2);

  network.routeMessages();
  const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

  require(!result.blockFinalized,
          "One of two validators must not finalize without peer votes.");
  require(runtime.blockchain().size() == 1,
          "An unfinalized candidate must remain outside the canonical chain.");
  require(!runtime.finalizationRegistry().hasFinalizedHeight(1),
          "The candidate height must remain unfinalized without quorum.");
}

void testSingleValidatorCandidateAppendsOnlyDuringFinalization() {
  const auto validators = makeValidators(1);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "candidate-finalization-append");
  node::NodeRuntime runtime = startRuntime(genesis);
  test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(validators.front().keyPair, provider);

  consensus::ConsensusEventLoop loop(runtime, network.mesh,
                                     network.validatedInbox, developmentPolicy(),
                                     provider);
  configureProducer(loop, signer);

  require(runtime.blockchain().size() == 1,
          "The test must begin with genesis only.");

  injectProposal(runtime, network.mesh, signer, kTimestamp + 2);
  network.routeMessages();
  const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

  require(result.blockFinalized,
          "A single-validator development chain must finalize its own quorum.");
  require(runtime.blockchain().size() == 2,
          "The finalized candidate must be appended exactly once.");
  require(runtime.finalizationRegistry().hasFinalizedHeight(1),
          "Finalization must be recorded in the runtime registry.");
}

void testMissingProposalStillAdvancesRound() {
  const auto validators = makeValidators(2);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "candidate-proposer-timeout");
  node::NodeRuntime runtime = startRuntime(genesis);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;

  consensus::ConsensusEventLoop loop(runtime, network.mesh,
                                     network.validatedInbox, developmentPolicy(),
                                     provider);

  const auto timeout =
      runtime.consensusRoundManager().roundTimeout().expiresAt();
  network.routeMessages();
  const consensus::ConsensusTickResult result = loop.tick(timeout);

  require(result.roundAdvanced,
          "Consensus must advance when the scheduled proposer sends no block.");
  require(runtime.consensusRoundManager().currentState().round() == 2,
          "The proposer timeout must move consensus to round 2.");
  require(runtime.blockchain().size() == 1,
          "A timeout without a proposal must not modify the blockchain.");
}

void testConsensusLoopPersistsSignedPrevoteBeforeBroadcast() {
  const auto validators = makeValidators(2);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "candidate-signed-prevote-recovery");
  node::NodeRuntime runtime = startRuntime(genesis);
  test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(proposerFixture(validators, runtime).keyPair,
                              provider);

  const std::filesystem::path recoveryPath =
      std::filesystem::temp_directory_path() /
      "nodo-consensus-event-loop-signed-prevote.state";
  std::error_code cleanupError;
  std::filesystem::remove(recoveryPath, cleanupError);

  consensus::ConsensusEventLoop loop(runtime, network.mesh,
                                     network.validatedInbox, developmentPolicy(),
                                     provider);
  loop.setRecoveryPath(recoveryPath);
  configureProducer(loop, signer);

  injectProposal(runtime, network.mesh, signer, kTimestamp + 2);
  network.routeMessages();
  const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

  require(!result.blockFinalized,
          "Two validators should not finalize with only the local prevote.");

  const auto stored = consensus::ConsensusRecoveryStore::load(recoveryPath);
  require(stored.has_value(),
          "Consensus loop must persist recovery state after local PREVOTE.");
  require(stored->votedPrevote(),
          "Recovery state must record that the local validator prevoted.");
  require(
      stored->persistedPrevote().has_value(),
      "Recovery state must contain the exact signed PREVOTE for rebroadcast.");
  require(stored->persistedPrevote()->decision() ==
              consensus::ValidatorVoteDecision::PREVOTE,
          "Persisted vote must be a PREVOTE.");
  require(
      stored->persistedPrevote()->verify(developmentPolicy(), provider),
      "Persisted PREVOTE must be a valid signed vote, not a boolean marker.");

  std::filesystem::remove(recoveryPath, cleanupError);
}
} // namespace

int main() {
  try {
    testCandidateDoesNotEnterChainWithoutQuorum();
    testSingleValidatorCandidateAppendsOnlyDuringFinalization();
    testConflictingVoteCreatesEvidenceImmediately();
    testMissingProposalStillAdvancesRound();
    testConsensusLoopPersistsSignedPrevoteBeforeBroadcast();
    std::cout << "Consensus event loop candidate tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Consensus event loop candidate tests FAILED: " << error.what()
              << "\n";
    return 1;
  }
}
