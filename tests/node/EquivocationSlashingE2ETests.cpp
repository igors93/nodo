// Real multi-node TCP end-to-end test (roadmap item 1.4, gate scenario 5):
// a scheduled proposer signs and broadcasts two different valid blocks at the
// same (height, round) — proposer equivocation. Honest peers
// must detect the conflicting signed proposals
// (ConsensusEventLoop::processBlockProposals already does this whenever it
// holds a pending candidate from a proposer and receives a second,
// different, validly-signed proposal from that same proposer for the same
// height/round), turn it into finalized ProposerEquivocationEvidence, and
// the already-existing canonical slashing pipeline
// (CanonicalSlashingTransition, wired into block finalization) must apply a
// real penalty to the offending validator's stake — observable via
// GET /stake/status/{validator} on any honest node.
//
// The malicious node is built directly around NodeOrchestrator (not
// NodeDaemon) so the test can drive the double proposal by hand; everything
// else about it (transport, gossip, consensus voting) is the same
// production code path every other node uses.

#include "../common/RealTcpNodeTestSupport.hpp"

#include "consensus/BlockProposalPhase.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/Peer.hpp"

namespace {

#ifndef _WIN32

std::optional<bool> stakeJailed(const NodeSpec &spec,
                                const std::string &validatorAddress) {
  const auto response =
      httpRequest(spec.rpcPort, "GET", "/stake/status/" + validatorAddress);
  if (!response.has_value() || response->statusCode != 200) {
    return std::nullopt;
  }
  return jsonBool(response->body, "jailed");
}

std::uint64_t stakeSlashedRawUnits(const NodeSpec &spec,
                                   const std::string &validatorAddress) {
  const auto response =
      httpRequest(spec.rpcPort, "GET", "/stake/status/" + validatorAddress);
  if (!response.has_value() || response->statusCode != 200) {
    return 0;
  }
  return jsonUnsigned(response->body, "slashedRawUnits").value_or(0);
}

// Runs a validator directly around NodeOrchestrator. Starts normally
// (transport, gossip, consensus loop, RPC), authenticates with its static
// peers, then — once — signs and broadcasts two different valid blocks at
// height 1 in the first round for which it is the scheduled proposer,
// before settling into normal honest ticking (voting like any other
// validator) for the rest of the process lifetime. Waiting for the actual
// scheduled round avoids racing startup/handshake time against round 1.
int runEquivocatingProposerChild(std::size_t nodeIndex, const NodeSpecs &specs,
                                 const config::GenesisConfig &genesis,
                                 const Topology &topology) {
  gChildStopRequested = 0;
  std::signal(SIGTERM, requestChildStop);
  std::signal(SIGINT, requestChildStop);

  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider validatorProvider;
  const NodeSpec &local = specs.at(nodeIndex);
  writeChildStatus(local, "STARTING");

  node::NodeOrchestratorConfig orchestratorConfig(
      genesis, node::NodeDataDirectoryConfig(local.dataDirectory),
      peerInfo(local, genesis.genesisTimestamp()),
      local.validatorKey.address().value(), local.rpcPort, "127.0.0.1",
      kConsensusTickMilliseconds,
      static_cast<std::size_t>(
          genesis.networkParameters().maxTransactionsPerBlock()));

  node::NodeOrchestrator orchestrator(orchestratorConfig, policy,
                                      validatorProvider);
  const crypto::Signer localSigner(local.validatorKey, validatorProvider);
  orchestrator.setLocalSigner(localSigner);
  orchestrator.setLocalNodeIdentity(local.identityKey);

  const node::NodeOrchestratorStartResult started = orchestrator.start();
  if (!started.running()) {
    writeChildStatus(local, "FAILED: orchestrator start: " + started.reason);
    std::cerr << local.nodeId << " failed to start: " << started.reason << '\n';
    return 2;
  }
  if (!orchestrator.rpcRunning()) {
    writeChildStatus(local, "FAILED: RPC server on port " +
                                std::to_string(local.rpcPort) + ": " +
                                orchestrator.rpcStartError());
    orchestrator.stop();
    return 3;
  }

  {
    const std::int64_t now = unixTime();
    for (const std::size_t peerIndex : topology.at(nodeIndex)) {
      const p2p::PeerMetadata meta(
          specs[peerIndex].nodeId,
          p2p::PeerEndpoint("127.0.0.1", specs[peerIndex].p2pPort), "", now,
          now, 0, false);
      orchestrator.registerBootstrapPeer(meta, now);
    }
  }

  writeChildStatus(local, "RUNNING");

  bool doubleProposed = false;
  bool mempoolSeeded = false;
  std::uint64_t equivocationRound = 0;
  while (!gChildStopRequested && orchestrator.isRunning()) {
    const std::int64_t now = unixTime();
    orchestrator.tick(now);

    std::size_t authenticatedNetworkPeers = 0;
    for (const NodeSpec &peer : specs) {
      if (peer.nodeId != local.nodeId &&
          orchestrator.mutableTcpRuntime().hasAuthenticatedSession(
              peer.nodeId)) {
        ++authenticatedNetworkPeers;
      }
    }

    const consensus::ConsensusRoundState &state =
        orchestrator.mutableRuntime().consensusRoundManager().currentState();

    // Seed the transaction only when this validator actually owns the active
    // proposer slot. Gossip the same transaction to the honest validators so
    // a later round can still produce a block after detecting equivocation.
    // The old fixture kept it private, making every honest proposer fail with
    // an empty mempool forever after round 1.
    if (!mempoolSeeded && authenticatedNetworkPeers == 2 &&
        state.height() == 1 &&
        state.proposerAddress() == local.validatorKey.address().value()) {
      const core::Transaction seedTransfer = signedTransfer(
          genesis, "equivocation", "equivocation-recipient", 1, now);
      const bool admitted =
          orchestrator.mutableRuntime()
              .mutableMempool()
              .admitTransaction(seedTransfer, policy,
                                crypto::SecurityContext::USER_TRANSACTION, now)
              .success();

      std::string gossipPayload;
      if (admitted && seedTransfer.hasSignatureBundle() &&
          !seedTransfer.signatureBundle().signatures().empty()) {
        gossipPayload = node::PersistentMempoolStore::serializeForGossip(
            seedTransfer,
            seedTransfer.signatureBundle().signatures().front().publicKey(),
            now);
      }
      if (!gossipPayload.empty()) {
        const p2p::GossipDeliveryReport delivery =
            orchestrator.gossipBroadcast(
                p2p::NetworkMessageType::TRANSACTION_GOSSIP, gossipPayload,
                now);
        mempoolSeeded = delivery.acceptedCount() > 0;
        if (mempoolSeeded) {
          equivocationRound = state.round();
        }
      }
    }

    if (!doubleProposed && mempoolSeeded && state.height() == 1 &&
        state.round() == equivocationRound &&
        state.proposerAddress() == local.validatorKey.address().value()) {
      const std::optional<core::Block> blockA =
          orchestrator.produceBlock(1, equivocationRound, now);
      const std::optional<core::Block> blockB =
          orchestrator.produceBlock(1, equivocationRound, now + 1);

      if (blockA.has_value() && blockB.has_value() &&
          blockA->hash() != blockB->hash()) {
        const consensus::BlockProposalResult proposalA =
            consensus::BlockProposalPhase::propose(
                *blockA, local.validatorKey.address().value(),
                equivocationRound, now, localSigner,
                orchestrator.mutableTcpRuntime().gossipMesh(),
                validatorProvider);
        if (proposalA.proposed()) {
          // BlockProposalPhase already broadcasts to peers. Inject only the
          // first proposal locally so the equivocator can participate without
          // sending either network proposal twice.
          orchestrator.gossipInjectLoopback(
              p2p::NetworkMessageType::BLOCK_PROPOSAL,
              proposalA.serializedProposal(), now);
        }

        const consensus::BlockProposalResult proposalB =
            consensus::BlockProposalPhase::propose(
                *blockB, local.validatorKey.address().value(),
                equivocationRound, now + 1, localSigner,
                orchestrator.mutableTcpRuntime().gossipMesh(),
                validatorProvider);

        doubleProposed = proposalA.proposed() && proposalB.proposed();
      }
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(kDaemonTickMilliseconds));
  }

  orchestrator.stop();
  return doubleProposed ? 0 : 4;
}

void testProposerEquivocationIsSlashedEndToEnd() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nodo-equivocation-e2e-" + std::to_string(::getpid()));
  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);

  const NodeSpecs specs = makeNodeSpecs(root, "equivocation");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "equivocation");
  const Topology topology = fullMeshTopology();

  const std::size_t maliciousIndex = scheduledProposerIndex(specs, genesis);
  std::array<std::size_t, 2> honestIndices{};
  std::size_t honestCount = 0;
  for (std::size_t index = 0; index < specs.size(); ++index) {
    if (index != maliciousIndex) {
      honestIndices[honestCount++] = index;
    }
  }
  require(honestCount == 2, "Expected exactly two honest validators.");
  const NodeSpec &honestA = specs[honestIndices[0]];
  const NodeSpec &honestB = specs[honestIndices[1]];
  const std::string maliciousAddress =
      specs[maliciousIndex].validatorKey.address().value();

  ChildProcesses honestNodes(specs, genesis, topology);
  pid_t maliciousPid = 0;

  try {
    honestNodes.start(honestIndices[0]);
    honestNodes.start(honestIndices[1]);
    maliciousPid = ::fork();
    if (maliciousPid < 0) {
      throw std::runtime_error("fork() failed for equivocating proposer.");
    }
    if (maliciousPid == 0) {
      const int result = runEquivocatingProposerChild(maliciousIndex, specs,
                                                      genesis, topology);
      ::_exit(result);
    }

    waitForRpc(honestA);
    waitForRpc(honestB);
    waitForRpc(specs[maliciousIndex]);

    require(waitUntil(90s,
                      [&] {
                        return hasAuthenticatedPeer(honestA) &&
                               hasAuthenticatedPeer(honestB);
                      }),
            "Honest validators did not authenticate with each other.");

    // The network must keep making progress despite the equivocation:
    // either round 1 splits (honest nodes locking onto different
    // conflicting blocks) and a later round finalizes, or one of the two
    // conflicting blocks reaches quorum directly. Either way, height 1
    // must eventually finalize.
    require(waitUntil(300s,
                      [&] {
                        const bool finalizedA =
                            reachedFinalizedHeight(honestA, 1);
                        const bool finalizedB =
                            reachedFinalizedHeight(honestB, 1);
                        return finalizedA && finalizedB;
                      }),
            "Honest validators did not finalize block 1 despite proposer "
            "equivocation.");

    // Ensure there is work for a subsequent proposer if the equivocation
    // evidence was detected too late to be included in block 1. Without this
    // transaction, an evidence-only follow-up block cannot be produced by the
    // current block-production policy.
    const core::Transaction continuationTransfer = signedTransfer(
        genesis, "equivocation", "equivocation-recipient", 2, unixTime());
    const auto continuationSubmitted =
        submitTransaction(honestA, continuationTransfer);
    require(continuationSubmitted.has_value() &&
                continuationSubmitted->statusCode == 200,
            "Continuation transaction submission did not return HTTP 200.");

    // Evidence must be detected, gossiped, finalized, and the canonical
    // slashing pipeline must apply a real penalty. This can land in a
    // later block than height 1, so poll with a generous budget while
    // the network keeps ticking and producing further blocks.
    require(waitUntil(
                360s,
                [&] {
                  const std::optional<bool> jailedOnA =
                      stakeJailed(honestA, maliciousAddress);
                  const std::optional<bool> jailedOnB =
                      stakeJailed(honestB, maliciousAddress);
                  return (jailedOnA.value_or(false) &&
                          jailedOnB.value_or(false)) ||
                         (stakeSlashedRawUnits(honestA, maliciousAddress) > 0 &&
                          stakeSlashedRawUnits(honestB, maliciousAddress) > 0);
                }),
            "The equivocating proposer was not jailed/slashed on both "
            "honest nodes after proposer equivocation was finalized.");

    stopPid(maliciousPid, specs[maliciousIndex].nodeId);
    maliciousPid = 0;
    honestNodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    stopPid(maliciousPid, specs[maliciousIndex].nodeId);
    honestNodes.stopAll();
    std::filesystem::remove_all(root, cleanupError);
    throw;
  }
}

#endif

} // namespace

int main() {
#ifdef _WIN32
  std::cout
      << "Equivocation slashing E2E test requires POSIX process control.\n";
  return 0;
#else
  try {
    testProposerEquivocationIsSlashedEndToEnd();
    std::cout << "Equivocation slashing end-to-end test passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Equivocation slashing end-to-end test FAILED: "
              << error.what() << '\n';
    return 1;
  }
#endif
}
