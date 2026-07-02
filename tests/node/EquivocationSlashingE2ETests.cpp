// Real multi-node TCP end-to-end test (roadmap item 1.4, gate scenario 5):
// the round-0 scheduled proposer signs and broadcasts two different valid
// blocks at the same (height, round) — proposer equivocation. Honest peers
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
// peers, then — once — signs and broadcasts two different valid blocks for
// (height=1, round=0), before settling into normal honest ticking (voting
// like any other validator) for the rest of the process lifetime. This node
// is always the round-0 scheduled proposer by construction, so no waiting
// for its turn is required.
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
  while (!gChildStopRequested && orchestrator.isRunning()) {
    const std::int64_t now = unixTime();
    orchestrator.tick(now);

    // A block only gets produced when the mempool holds at least one
    // pending transaction (empty blocks are refused outside
    // epoch-settlement heights). Admit one directly into this node's own
    // mempool — bypassing RPC/gossip, since this node's own local state is
    // all that matters for producing its own candidate blocks — so both
    // produceBlock() calls below succeed. Both candidates end up carrying
    // the same single transaction; they still differ in hash because they
    // carry different timestamps.
    if (!mempoolSeeded && hasAuthenticatedPeer(local)) {
      const core::Transaction seedTransfer = signedTransfer(
          genesis, "equivocation", "equivocation-recipient", 1, now);
      mempoolSeeded =
          orchestrator.mutableRuntime()
              .mutableMempool()
              .admitTransaction(seedTransfer, policy,
                                crypto::SecurityContext::USER_TRANSACTION, now)
              .success();
    }

    if (!doubleProposed && mempoolSeeded) {
      const std::optional<core::Block> blockA =
          orchestrator.produceBlock(1, 0, now);
      const std::optional<core::Block> blockB =
          orchestrator.produceBlock(1, 0, now + 1);

      if (blockA.has_value() && blockB.has_value() &&
          blockA->hash() != blockB->hash()) {
        const consensus::BlockProposalResult proposalA =
            consensus::BlockProposalPhase::propose(
                *blockA, local.validatorKey.address().value(), 0, now,
                localSigner, orchestrator.mutableTcpRuntime().gossipMesh(),
                validatorProvider);
        if (proposalA.proposed()) {
          orchestrator.gossipBroadcast(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                       proposalA.serializedProposal(), now);
        }

        const consensus::BlockProposalResult proposalB =
            consensus::BlockProposalPhase::propose(
                *blockB, local.validatorKey.address().value(), 0, now + 1,
                localSigner, orchestrator.mutableTcpRuntime().gossipMesh(),
                validatorProvider);
        if (proposalB.proposed()) {
          orchestrator.gossipBroadcast(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                       proposalB.serializedProposal(), now + 1);
        }

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
    // either round 0 splits (honest nodes locking onto different
    // conflicting blocks) and a later round finalizes, or one of the two
    // conflicting blocks reaches quorum directly. Either way, height 1
    // must eventually finalize.
    require(waitUntil(300s,
                      [&] {
                        return reachedFinalizedHeight(honestA, 1) &&
                               reachedFinalizedHeight(honestB, 1);
                      }),
            "Honest validators did not finalize block 1 despite proposer "
            "equivocation.");

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
