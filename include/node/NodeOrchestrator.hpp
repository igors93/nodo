#ifndef NODO_NODE_NODE_ORCHESTRATOR_HPP
#define NODO_NODE_NODE_ORCHESTRATOR_HPP

#include "config/NetworkParameters.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/EvidencePool.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRpcServer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SyncHealth.hpp"
#include "node/TcpTestnetNodeRuntime.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/Peer.hpp"
#include "p2p/PeerReconnectionPolicy.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace nodo::storage {
class SlashingEvidenceStore;
}

namespace nodo::p2p {
class DiscoveryService;
}

namespace nodo::node {

/*
 * NodeOrchestratorConfig holds all the parameters needed to start a node.
 *
 * Separating configuration from runtime state makes it easy to construct
 * the node from a config file without mixing lifecycle concerns.
 */
class NodeOrchestratorConfig {
public:
  NodeOrchestratorConfig();

  NodeOrchestratorConfig(config::GenesisConfig genesisConfig,
                         NodeDataDirectoryConfig dataDirectory,
                         p2p::PeerInfo localPeer,
                         std::string localValidatorAddress,
                         std::uint16_t rpcPort, std::string rpcBindAddr,
                         std::int64_t consensusTickMs,
                         std::size_t maxBlockTransactions,
                         bool enablePeerExchange = true);

  const config::GenesisConfig &genesisConfig() const;
  const NodeDataDirectoryConfig &dataDirectory() const;
  const p2p::PeerInfo &localPeer() const;
  const std::string &localValidatorAddress() const;
  std::uint16_t rpcPort() const;
  const std::string &rpcBindAddr() const;
  std::int64_t consensusTickMs() const;
  std::size_t maxBlockTransactions() const;
  bool enablePeerExchange() const;

  bool isValid() const;

private:
  config::GenesisConfig m_genesisConfig;
  NodeDataDirectoryConfig m_dataDirectory;
  p2p::PeerInfo m_localPeer;
  std::string m_localValidatorAddress;
  std::uint16_t m_rpcPort;
  std::string m_rpcBindAddr;
  std::int64_t m_consensusTickMs;
  std::size_t m_maxBlockTransactions;
  bool m_enablePeerExchange;
};

enum class NodeOrchestratorStartStatus {
  RUNNING,
  ALREADY_RUNNING,
  INIT_FAILED,
  STATE_LOAD_FAILED,
  TRANSPORT_FAILED,
  CONSENSUS_FAILED
};

std::string
nodeOrchestratorStartStatusToString(NodeOrchestratorStartStatus status);

struct NodeOrchestratorStartResult {
  NodeOrchestratorStartStatus status;
  std::string reason;
  bool freshGenesis = false;

  bool running() const {
    return status == NodeOrchestratorStartStatus::RUNNING;
  }
};

/*
 * NodeOrchestrator is the top-level lifecycle manager for a Nodo node.
 *
 * Startup sequence:
 *   1. Detect if data directory is initialized; if not, run genesis init.
 *   2. Load NodeRuntime from disk (blocks + mempool + manifest).
 *   3. Start TcpTestnetNodeRuntime (bind TCP port, start gossip).
 *   4. Start ConsensusEventLoop (background thread with block producer wired).
 *   5. Start NodeRpcServer (HTTP server for operator queries).
 *
 * Tick loop (call from your main loop or run via runBlocking()):
 *   - Drives gossip transport (receive/send)
 *   - Processes peer handshakes (auto-register new peers)
 *   - Applies incoming block announcements
 *   - Serves/receives block sync requests
 *
 * Shutdown sequence (reverse order):
 *   - RPC server → ConsensusEventLoop → TcpTestnet → state flush
 *
 * Thread model:
 *   - ConsensusEventLoop runs on its own thread.
 *   - NodeRpcServer runs on its own thread.
 *   - The gossip/network tick is driven by the caller's thread or
 * runBlocking().
 */
class NodeOrchestrator {
public:
  static constexpr std::int64_t DEFAULT_TICK_INTERVAL_MS = 50;

  explicit NodeOrchestrator(NodeOrchestratorConfig config,
                            const crypto::CryptoPolicy &policy,
                            const crypto::SignatureProvider &provider);

  ~NodeOrchestrator();

  // ---- Lifecycle --------------------------------------------------------

  /*
   * Initialize, load state, start all subsystems.
   * Returns immediately after all threads are started.
   */
  NodeOrchestratorStartResult start();

  /*
   * Run the network tick loop on the calling thread until stop() is called.
   * Blocks the caller. Meant for single-threaded integration and tests.
   */
  void runBlocking(std::int64_t tickIntervalMs = DEFAULT_TICK_INTERVAL_MS);

  /*
   * Signal all subsystems to stop and wait for threads to exit.
   * Safe to call multiple times.
   */
  void stop();

  bool isRunning() const;

  // ---- Block production signer ------------------------------------------

  /*
   * Inject the local validator signer used for block production.
   * Must be called before the consensus loop starts proposing blocks.
   */
  void setLocalSigner(crypto::Signer signer);

  void setLocalNodeIdentity(crypto::KeyPair nodeIdentityKey);

  // ---- Single tick (usable from tests without a background thread) ------

  /*
   * Execute one network tick:
   *   - gossip receive/flush
   *   - peer handshake registration
   *   - block announce processing
   *   - block sync (serve requests + apply responses)
   */
  void tick(std::int64_t now);

  // Block producer callback wired into ConsensusEventLoop.
  // Returns the validated candidate block, or nullopt on failure.
  // Does NOT finalize — finalization is handled entirely by ConsensusEventLoop.
  std::optional<core::Block>
  produceBlock(std::uint64_t height, std::uint64_t round, std::int64_t now);

  // ---- Accessors (read-only for monitoring / tests) ---------------------

  const NodeRuntime &runtime() const;
  NodeRuntime &mutableRuntime();
  const TcpTestnetNodeRuntime &tcpRuntime() const;
  TcpTestnetNodeRuntime &mutableTcpRuntime();
  const NodeOrchestratorConfig &config() const;
  const consensus::EvidencePool &evidencePool() const;
  const crypto::CryptoPolicy &cryptoPolicy() const;
  const crypto::SignatureProvider &signatureProvider() const;
  const SyncHealth &syncHealth() const;
  bool rpcRunning() const;
  const std::string &rpcStartError() const;

  // ---- Gossip helpers for NodeDaemon ------------------------------------

  // Drain all messages of `type` from the gossip inbox and return them.
  std::vector<p2p::NetworkEnvelope>
  drainGossipInbox(p2p::NetworkMessageType type);

  // Broadcast a message to all connected peers via the gossip mesh.
  p2p::GossipDeliveryReport gossipBroadcast(p2p::NetworkMessageType type,
                                            const std::string &payload,
                                            std::int64_t now);

  // Inject a message directly into the local gossip mesh inbox.
  void gossipInjectLoopback(p2p::NetworkMessageType type,
                            const std::string &payload, std::int64_t now);

  // Register a bootstrap or operator-provided peer as a network candidate.
  // The actual TCP attempt is made only through the reconnection policy, so
  // bootstrap and discovery obey the same backoff/quarantine rules.
  void registerBootstrapPeer(const p2p::PeerMetadata &peer, std::int64_t now);

  // Compatibility entry point for tests and daemon helpers. It now feeds the
  // real reconnection policy instead of bypassing discovery/backoff.
  void addAndConnectPeer(const p2p::PeerMetadata &peer, std::int64_t now);

  const p2p::PeerReconnectionPolicy &reconnectionPolicy() const;

  // Evaluate a remote peer's ChainStatusMessage and broadcast a
  // BLOCK_SYNC_REQUEST if the durable sync checkpoint is behind.
  void triggerSyncIfBehind(const ChainStatusMessage &remotePeerStatus,
                           const std::string &remotePeerId, std::int64_t now);

private:
  NodeOrchestratorConfig m_config;
  const crypto::CryptoPolicy &m_policy;
  const crypto::SignatureProvider &m_provider;

  // Core state (populated in start())
  std::unique_ptr<NodeRuntime> m_runtime;
  std::unique_ptr<TcpTestnetNodeRuntime> m_tcpRuntime;
  std::unique_ptr<consensus::ConsensusEventLoop> m_consensusLoop;
  std::unique_ptr<NodeRpcServer> m_rpcServer;
  std::string m_rpcStartError;
  std::unique_ptr<storage::SlashingEvidenceStore> m_slashingEvidenceStore;
  consensus::EvidencePool m_evidencePool;
  std::unique_ptr<p2p::DiscoveryService> m_discoveryService;
  p2p::PeerReconnectionPolicy m_reconnectionPolicy;
  std::map<std::string, p2p::PeerEndpoint> m_reconnectEndpoints;
  std::set<std::string> m_discoverySeededPeers;
  std::int64_t m_lastPeerExchangeBroadcastAt = 0;

  std::atomic<bool> m_running;
  std::optional<crypto::Signer> m_localSigner;
  std::optional<crypto::KeyPair> m_localNodeIdentity;
  SyncHealth m_syncHealth;

  // ---- Internal startup helpers ----------------------------------------

  NodeOrchestratorStartResult initOrLoad();
  bool startTransport();
  bool startConsensus();
  bool startRpc();

  // Build chain status from current runtime state.
  ChainStatusMessage currentChainStatus() const;

  std::optional<p2p::PeerMetadata> localHandshakePeer(std::int64_t now) const;

  TcpTestnetNodeRuntimeConfig buildTransportConfig() const;

  void trackPeerCandidate(const std::string &nodeId,
                          const p2p::PeerEndpoint &endpoint, std::int64_t now,
                          bool immediateRetry);

  void seedDiscoveryPeer(const std::string &nodeId,
                         const p2p::PeerEndpoint &endpoint);

  void handleDiscoveredPeer(const std::string &peerId, const std::string &host,
                            std::uint16_t tcpPort, std::int64_t now);

  void processPeerExchangeMessages(std::int64_t now);

  void broadcastPeerExchange(std::int64_t now);

  std::vector<p2p::PeerSubnetInfo> activePeerSubnets() const;

  std::vector<p2p::PeerExchangeEntry>
  loadPersistedPeerExchangeCandidates() const;

  void persistPeerExchangeCandidates(
      const std::vector<p2p::PeerExchangeEntry> &acceptedEntries) const;

  void driveNetworkPeerPolicy(std::int64_t now);

  std::optional<p2p::PeerEndpoint>
  endpointForReconnectCandidate(const p2p::PeerReconnectionState &state) const;

  bool attemptReconnectCandidate(const p2p::PeerReconnectionState &state,
                                 std::int64_t now);
};

} // namespace nodo::node

#endif
