#ifndef NODO_NODE_NODE_DAEMON_HPP
#define NODO_NODE_NODE_DAEMON_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/NodeOrchestrator.hpp"
#include "node/SeenTransactionCache.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * NodeDaemonConfig adds the peer list and listen address to the orchestrator
 * config. These are the flags supplied on the command line when starting a
 * daemon node.
 */
struct NodeDaemonPeerEntry {
  std::string nodeId;
  std::string host;
  std::uint16_t port = 0;

  bool isValid() const { return !nodeId.empty() && !host.empty() && port > 0; }
};

struct NodeDaemonConfig {
  NodeOrchestratorConfig orchestratorConfig;
  std::vector<NodeDaemonPeerEntry> staticPeers;
  std::size_t seenCacheMaxEntries = SeenTransactionCache::DEFAULT_MAX_ENTRIES;
  std::int64_t seenCacheTtlSeconds = SeenTransactionCache::DEFAULT_TTL_SECONDS;
};

/*
 * NodeDaemon is the top-level runtime for a continuously-running distributed
 * validator node.
 *
 * It extends NodeOrchestrator with:
 *   - static peer registration from the command line (--peer flags);
 *   - transaction gossip: receive TRANSACTION_GOSSIP from peers, validate,
 *     insert into local mempool, and re-broadcast to other peers;
 *   - transaction and finalized-artifact network orchestration;
 *   - block proposals are consumed by ConsensusEventLoop so unfinalized
 *     candidates never enter the canonical chain from the daemon thread;
 *   - finalized artifact reception: verify FINALIZED_BLOCK_ARTIFACT messages
 *     from peers and record finality in the local registry.
 *
 * Usage (single blocking call from the CLI):
 *
 *   NodeDaemon daemon(config, policy, provider);
 *   auto result = daemon.start();
 *   if (!result.running()) { ... handle error ... }
 *   daemon.runBlocking();   // blocks until SIGINT or stop()
 */
class NodeDaemon {
public:
  static constexpr std::int64_t DEFAULT_TICK_INTERVAL_MS = 50;

  NodeDaemon(NodeDaemonConfig config, const crypto::CryptoPolicy &policy,
             const crypto::SignatureProvider &provider);

  ~NodeDaemon();

  // Start the daemon: init/load state, bind transport, start consensus,
  // register static peers, start background threads. Returns immediately.
  NodeOrchestratorStartResult start();

  // Run the network tick loop on the calling thread until stop() is called.
  void runBlocking(std::int64_t tickIntervalMs = DEFAULT_TICK_INTERVAL_MS);

  // Signal all subsystems to stop.
  void stop();

  bool isRunning() const;

  // Inject a validator signer (Ed25519/BLS) so this node can vote and propose.
  void setLocalSigner(crypto::Signer signer);

  void setLocalNodeIdentity(crypto::KeyPair nodeIdentityKey);

  // Execute one combined tick: network + gossip + new message types.
  void tick(std::int64_t now);

  const NodeOrchestrator &orchestrator() const;

private:
  NodeDaemonConfig m_config;
  const crypto::CryptoPolicy &m_policy;
  const crypto::SignatureProvider &m_provider;
  NodeOrchestrator m_orchestrator;
  SeenTransactionCache m_seenTxCache;
  std::atomic<bool> m_running;
  std::pair<std::uint64_t, std::uint64_t> m_lastProposedRound = {0, 0};
  std::pair<std::uint64_t, std::uint64_t> m_lastProposalAttemptRound = {0, 0};
  std::int64_t m_lastProposalAttemptAt = 0;
  std::optional<crypto::Signer> m_localSigner;
  
  std::uint32_t m_txRelayBudgetCounter = 0;
  std::int64_t m_txRelayBudgetSecond = 0;
  std::uint64_t m_txRelayDroppedCount = 0;

  // Register static peers into the transport and gossip mesh.
  void registerStaticPeers(std::int64_t now);

  // Handle incoming TRANSACTION_GOSSIP messages.
  void processTransactionGossip(std::int64_t now);

  // Handle incoming FINALIZED_BLOCK_ARTIFACT messages. When an artifact
  // references a height this node does not yet have, this triggers a
  // persistent block sync request instead of silently discarding it, so a
  // node that fell behind (e.g. after a restart, or after missing rounds
  // while the rest of the network kept finalizing without a round timeout)
  // recovers instead of waiting forever for a proposal at its stale height.
  void processFinalizedArtifacts(std::int64_t now);

  // Check if local node is proposer and initiate block proposal if true.
  void maybeProposeBlock(std::int64_t now);
};

} // namespace nodo::node

#endif
