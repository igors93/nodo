// In-process consensus test for multi-hop relay (loopback transport, so it
// runs on every platform).
//
// Four equal-weight validators are connected only as a line A-B-C-D, with
// the scheduled proposer at one end. The strict quorum floor(2W/3)+1
// (ADR 0002) needs three of the four votes, but every node has a direct
// session with at most two others and the end nodes with only one. Without
// relay the far end never sees the proposal and the proposer never sees the
// far end's vote, so finality is impossible. ConsensusEventLoop forwards each
// newly accepted proposal and vote once, so all four must finalize the same
// block without any message being relayed twice or tripping a rate limit.

#include "../common/ConsensusPhaseTestFixtures.hpp"
#include "../common/TestFramework.hpp"
#include "config/NetworkParameters.hpp"
#include "config/ProtocolVersion.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/QuorumThreshold.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/consensus/BlockProductionPhase.hpp"
#include "node/consensus/BlockProposalPhase.hpp"
#include "node/consensus/ConsensusEventLoop.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/Peer.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;
using nodo::test::require;

constexpr std::size_t kNodeCount = 4;
constexpr std::int64_t kGenesisTimestamp = 1900800000;
constexpr std::int64_t kTransactionTimestamp = kGenesisTimestamp + 10;
constexpr std::uint32_t kPeerWindowLimit = 100;
constexpr std::uint16_t kBasePort = 32201;

const crypto::CryptoPolicy &developmentPolicy() {
  static const crypto::CryptoPolicy policy =
      crypto::CryptoPolicy::developmentPolicy();
  return policy;
}

struct NodeSpec {
  std::string nodeId;
  crypto::KeyPair validatorKey;
};

std::vector<NodeSpec> makeNodeSpecs() {
  std::vector<NodeSpec> specs;
  specs.reserve(kNodeCount);
  for (std::size_t index = 0; index < kNodeCount; ++index) {
    const std::string suffix(1, static_cast<char>('a' + index));
    specs.push_back(NodeSpec{"relay-line-node-" + suffix,
                             crypto::KeyPair::createDeterministicBls12381KeyPair(
                                 "consensus-relay-line-validator-" + suffix)});
  }
  return specs;
}

crypto::KeyPair userKey() {
  return test::consensusTestUserKey("consensus-relay-line-user");
}

config::GenesisConfig makeGenesis(const std::vector<NodeSpec> &specs) {
  std::vector<config::BootstrapValidatorConfig> validators;
  validators.reserve(specs.size());
  for (const NodeSpec &spec : specs) {
    validators.emplace_back(spec.validatorKey.publicKey(), 1, 1'000'000,
                            "consensus-relay-line-" + spec.nodeId);
  }

  return config::GenesisConfig(config::NetworkParameters::developmentLocal(),
                               kGenesisTimestamp, validators,
                               {test::fundedConsensusTestAccount(userKey())},
                               "consensus-relay-line-genesis");
}

node::NodeRuntime startRuntime(const config::GenesisConfig &genesis,
                               const NodeSpec &spec, std::size_t index) {
  const p2p::PeerInfo peer(
      spec.nodeId,
      "127.0.0.1:" + std::to_string(kBasePort + static_cast<int>(index)),
      config::kProtocolVersion, 0, kGenesisTimestamp);
  const node::NodeRuntimeStartResult result =
      node::NodeRuntimeFactory::startFromGenesis(
          node::NodeRuntimeConfig(genesis, peer, 16));
  require(result.started(),
          "Runtime start failed for " + spec.nodeId + ": " + result.reason());
  return result.runtime();
}

class TestNode {
public:
  TestNode(const NodeSpec &nodeSpec, std::size_t nodeIndex,
           const config::GenesisConfig &genesis,
           const std::shared_ptr<p2p::LoopbackTransportBus> &bus,
           const crypto::Bls12381SignatureProvider &provider)
      : spec(nodeSpec), index(nodeIndex),
        runtime(startRuntime(genesis, nodeSpec, nodeIndex)), transport(bus),
        mesh(p2p::GossipMeshConfig(
                 nodeSpec.nodeId, genesis.networkParameters().networkName(),
                 genesis.networkParameters().chainId(),
                 config::kProtocolVersion, genesis.deterministicId(), 120, 8,
                 kPeerWindowLimit, 50),
             transport),
        signer(nodeSpec.validatorKey, provider),
        consensusLoop(runtime, mesh, validatedInbox, developmentPolicy(),
                      provider) {
    consensusLoop.setLocalValidatorAddress(signer.address());
    consensusLoop.setLocalSigner(&signer);
  }

  NodeSpec spec;
  std::size_t index;
  node::NodeRuntime runtime;
  p2p::LoopbackTransport transport;
  p2p::GossipMesh mesh;
  p2p::GossipInbox validatedInbox;
  crypto::Signer signer;
  consensus::ConsensusEventLoop consensusLoop;
  std::vector<std::string> neighborIds;
  std::uint64_t votesRelayed = 0;
  std::uint64_t proposalsRelayed = 0;
};

p2p::PeerMetadata peerMetadata(const TestNode &node) {
  return p2p::PeerMetadata(
      node.spec.nodeId,
      p2p::PeerEndpoint("127.0.0.1", static_cast<std::uint16_t>(
                                         kBasePort + node.index)),
      "fingerprint-" + node.spec.nodeId, kGenesisTimestamp, kGenesisTimestamp,
      0, false);
}

void connectNodes(TestNode &left, TestNode &right) {
  require(left.mesh.registerPeer(peerMetadata(right)).success() &&
              right.mesh.registerPeer(peerMetadata(left)).success(),
          "Unable to register " + left.spec.nodeId + " <-> " +
              right.spec.nodeId);
  require(left.mesh.connectPeer(right.spec.nodeId).success() &&
              right.mesh.connectPeer(left.spec.nodeId).success(),
          "Unable to connect " + left.spec.nodeId + " <-> " +
              right.spec.nodeId);
  left.neighborIds.push_back(right.spec.nodeId);
  right.neighborIds.push_back(left.spec.nodeId);
}

std::size_t scheduledProposerIndex(
    const std::vector<std::unique_ptr<TestNode>> &nodes,
    const config::GenesisConfig &genesis) {
  const node::NodeRuntime &runtime = nodes.front()->runtime;
  const consensus::ConsensusRoundState &state =
      runtime.consensusRoundManager().currentState();
  const std::string proposer = consensus::ProposerSchedule::selectProposer(
      runtime.validatorRegistry(), genesis.networkParameters().chainId(),
      state.height(), state.round());
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (nodes[index]->signer.address() == proposer) {
      return index;
    }
  }
  throw std::runtime_error("Scheduled proposer is absent from the fixture.");
}

// One hop per step: every node flushes, then every node receives, then every
// consensus loop ticks. A fixed clock keeps all nodes inside round 1, so the
// result depends only on relay, never on a round timeout.
bool runUntilAllFinalized(const std::vector<TestNode *> &line,
                          std::int64_t now) {
  for (int step = 0; step < 60; ++step) {
    for (TestNode *node : line) {
      node->mesh.flushOutbound(now);
    }
    for (TestNode *node : line) {
      node->mesh.receiveAvailable(now);
      for (const p2p::NetworkEnvelope &message : node->mesh.drainAllInbox()) {
        node->validatedInbox.add(message);
      }
    }

    bool allFinalized = true;
    for (TestNode *node : line) {
      const consensus::ConsensusTickResult result =
          node->consensusLoop.tick(now);
      require(!result.hasError(), node->spec.nodeId +
                                      " consensus failed: " +
                                      result.errorMessage);
      node->votesRelayed += result.votesRelayed;
      node->proposalsRelayed += result.proposalsRelayed;
      allFinalized = allFinalized && node->runtime.blockchain().size() == 2;
    }
    if (allFinalized) {
      return true;
    }
  }
  return false;
}

void testLineTopologyFinalizesThroughRelay() {
  const std::vector<NodeSpec> specs = makeNodeSpecs();
  const config::GenesisConfig genesis = makeGenesis(specs);
  const auto bus = std::make_shared<p2p::LoopbackTransportBus>();
  const crypto::Bls12381SignatureProvider provider;

  std::vector<std::unique_ptr<TestNode>> nodes;
  for (std::size_t index = 0; index < specs.size(); ++index) {
    nodes.push_back(std::make_unique<TestNode>(specs[index], index, genesis,
                                               bus, provider));
  }

  // Proposer first: its proposal must cross three hops to reach the far end,
  // and the far end's votes three hops back.
  const std::size_t proposerIndex = scheduledProposerIndex(nodes, genesis);
  std::vector<TestNode *> line{nodes[proposerIndex].get()};
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    if (index != proposerIndex) {
      line.push_back(nodes[index].get());
    }
  }
  for (std::size_t position = 0; position + 1 < line.size(); ++position) {
    connectNodes(*line[position], *line[position + 1]);
  }

  const core::ValidatorRegistry &validatorSet =
      line.front()->runtime.validatorRegistry();
  const std::uint64_t requiredWeight = consensus::QuorumThreshold::requiredWeight(
      validatorSet.totalConsensusWeight());
  const std::uint64_t validatorWeight =
      validatorSet.consensusWeightFor(line.front()->signer.address());
  require(requiredWeight > 2 * validatorWeight,
          "Fixture error: an end node plus its only neighbor must fall short "
          "of quorum, otherwise the test would not depend on relay.");

  for (TestNode *node : line) {
    test::admitConsensusTestTransfer(node->runtime, userKey(), 1,
                                     kTransactionTimestamp);
  }

  TestNode &proposer = *line.front();
  const std::uint64_t round = 1;
  const std::int64_t proposedAt = kTransactionTimestamp + 15;
  const consensus::BlockCandidateResult candidate =
      consensus::BlockProductionPhase::produce(
          proposer.runtime,
          node::RuntimeBlockPipelineConfig(16, 1, round, proposedAt));
  require(candidate.produced(), "Proposer failed to produce a candidate.");
  const consensus::BlockProposalResult proposal =
      consensus::BlockProposalPhase::propose(
          candidate.block(), proposer.signer.address(), round, proposedAt,
          proposer.signer, proposer.mesh, provider);
  require(proposal.proposed(), "Proposer failed to broadcast its proposal.");
  proposer.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                   proposal.serializedProposal(), proposedAt);

  // The test controls time explicitly: start round 1 when the proposal is
  // delivered, so its old genesis-time timeout cannot discard the candidate.
  for (TestNode *node : line) {
    auto &manager = node->runtime.mutableConsensusRoundManager();
    const auto &state = manager.currentState();
    manager.advanceToHeight(state.height(), state.round(),
                            state.proposerAddress(), proposedAt,
                            genesis.networkParameters().targetBlockTimeSeconds());
  }
  const std::int64_t now = proposedAt;
  if (!runUntilAllFinalized(line, now)) {
    std::string heights;
    for (const TestNode *node : line) {
      heights += " " + node->spec.nodeId + "=" +
                 std::to_string(node->runtime.blockchain().size() - 1);
    }
    throw std::runtime_error(
        "Validators on the line did not all finalize block 1:" + heights);
  }

  const std::string finalizedHash =
      proposer.runtime.blockchain().latestBlock().hash();
  require(finalizedHash == candidate.block().hash(),
          "The line finalized a block other than the proposal.");
  for (const TestNode *node : line) {
    require(node->runtime.blockchain().latestBlock().hash() == finalizedHash,
            node->spec.nodeId + " finalized a different block.");
    const consensus::FinalizedBlockRecord *record =
        node->runtime.finalizationRegistry().recordForHeight(1);
    require(record != nullptr &&
                record->quorumCertificate().voteCount() >= 3,
            node->spec.nodeId + " has no quorum certificate with 3+ votes.");

    // At most one PREVOTE and one PRECOMMIT per other validator, and the one
    // proposal: anything more would mean a message was relayed twice.
    require(node->votesRelayed <= 2 * (kNodeCount - 1),
            node->spec.nodeId + " relayed some vote more than once.");
    require(node->proposalsRelayed <= 1,
            node->spec.nodeId + " relayed the proposal more than once.");
    for (const std::string &neighbor : node->neighborIds) {
      require(node->mesh.invalidMessageCountForPeer(neighbor) == 0,
              node->spec.nodeId + " penalized honest relay traffic from " +
                  neighbor + ".");
    }
  }

  // The proposer's own proposal arrives by loopback and was already
  // broadcast; the far end's only peer is the one that delivered it.
  require(line.front()->proposalsRelayed == 0,
          "The proposer must not relay its own loopback proposal.");
  require(line.back()->proposalsRelayed == 0 &&
              line.back()->votesRelayed == 0,
          "An end node must not echo messages back to their sender.");
  for (std::size_t position = 1; position + 1 < line.size(); ++position) {
    require(line[position]->proposalsRelayed == 1,
            line[position]->spec.nodeId +
                " must forward the proposal exactly once.");
    require(line[position]->votesRelayed > 0,
            line[position]->spec.nodeId + " must forward votes downstream.");
  }

  // A peer forwards one copy of every validator's votes, so the per-peer
  // VALIDATOR_VOTE window must have been scaled to the validator set: a burst
  // above the single-validator budget must still be accepted.
  TestNode &sender = *line[0];
  TestNode &receiver = *line[1];
  const std::uint32_t burst = 2 * kPeerWindowLimit;
  for (std::uint32_t index = 0; index < burst; ++index) {
    require(sender.mesh
                    .broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE,
                               "rate-probe-" + std::to_string(index), now)
                    .acceptedCount() == 1,
            "Rate probe was not queued.");
  }
  sender.mesh.flushOutbound(now);
  require(receiver.mesh.receiveAvailable(now).rejectedCount() == 0,
          "VALIDATOR_VOTE window was not scaled for relayed votes.");
}

} // namespace

int main() {
  try {
    testLineTopologyFinalizesThroughRelay();
    std::cout << "Consensus relay line-topology test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Consensus relay line-topology test FAILED: " << error.what()
              << '\n';
    return 1;
  }
}
