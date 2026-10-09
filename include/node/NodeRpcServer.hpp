#ifndef NODO_NODE_NODE_RPC_SERVER_HPP
#define NODO_NODE_NODE_RPC_SERVER_HPP

#include "node/HealthCheckService.hpp"
#include "node/JsonRpcServer.hpp"
#include "node/LightClientService.hpp"
#include "node/NodeEventBus.hpp"
#include "node/NodeMetrics.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PrometheusExporter.hpp"
#include "node/SyncHealth.hpp"
#include "node/WebSocketFrameCodec.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/PeerRateLimiter.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nodo::node {

class NodeDataDirectoryConfig;

/*
 * NodeRpcServer is the node HTTP surface. JSON-RPC is the official public
 * protocol API and is exposed through POST /rpc. The older REST routes remain
 * available as operational/development endpoints for status, health, metrics,
 * diagnostics and backward-compatible integration tests.
 *
 * Transport: standalone Asio. A fixed pool of worker threads runs one
 *   io_context; every connection is an asynchronous session on its own
 *   strand, so no thread is tied to a connection.
 * Protocol:  HTTP/1.x, one request per connection (Connection: close), JSON
 *   response bodies; GET /events upgrades to a WebSocket event stream.
 * Auth:      None — bind to loopback by default.
 * Limits (see Limits): concurrent connections and WebSocket sessions are
 *   capped (connections beyond the cap are closed at accept); the whole
 *   request must arrive within requestTimeout, so a client trickling bytes
 *   cannot hold a session open; malformed or oversized requests get a 4xx
 *   status instead of a silent close.
 * Rate limit: per-source-IP request cap (see MAX_REQUESTS_PER_WINDOW /
 *   RATE_LIMIT_WINDOW_SECONDS) to bound resource exhaustion from a local
 *   process hammering the endpoint; excess requests get HTTP 429. The
 *   window is fixed (not sliding), so budget sized only for casual polling
 *   leaves legitimate pollers (dashboards, integration test harnesses)
 *   locked out for the rest of the window once tripped.
 *
 * JSON-RPC endpoint:
 *   POST /rpc                  — JSON-RPC 2.0 public protocol API
 *
 * REST endpoints (operational/backward-compatible; all read-only except
 * /submit): GET  /status               — node height, round, peer count,
 * mempool size GET  /health               — structured health report
 *   GET  /metrics              — JSON metrics snapshot
 *   GET  /metrics/prometheus   — Prometheus text exposition
 *   GET  /block/{height}       — serialized block at given height
 *   GET  /tx/{txId}            — ledger record with matching id or sourceId
 *   GET  /account/{address}    — balance and nonce for address
 *   GET  /account/{address}/proof — Merkle inclusion proof of the address's
 *        balance/nonce against the current account-state root
 *   GET  /validators            — list of active validator addresses
 *   GET  /stake/status/{validator}
 *   GET  /stake/positions/{owner}
 *   GET  /stake/position/{positionId}
 *   GET  /stake/pending-unbonding/{validator}
 *   GET  /stake/validator/{validator}
 *   GET  /stake/audit
 *   GET  /peers                — list of known peers
 *   GET  /mempool              — current mempool transaction count and top txs
 *   GET  /governance/status    — proposal counts and governed parameters
 *   GET  /governance/proposals — proposal ids
 *   GET  /governance/proposal/{id}
 *   GET  /governance/votes/{id}
 *   GET  /governance/tally/{id}
 *   GET  /governance/decision/{id}
 *   GET  /governance/execution/{id}
 *   POST /submit               — admit a signed transaction submission envelope
 *
 * Error responses use HTTP 4xx with a JSON body:
 *   {"error": "<message>"}
 */
class NodeRpcServer {
public:
  static constexpr std::uint16_t DEFAULT_PORT = 8545;
  static constexpr std::size_t MAX_REQUEST_LEN = 65536;
  static constexpr std::size_t MAX_HEADER_LEN = 16384;
  static constexpr std::uint32_t MAX_REQUESTS_PER_WINDOW = 3000;
  static constexpr std::uint64_t RATE_LIMIT_WINDOW_SECONDS = 60;

  struct Limits {
    std::size_t workerThreads = 4;
    std::size_t maxConnections = 128;      // HTTP and WebSocket together
    std::size_t maxWebSocketSessions = 32; // counted within maxConnections
    std::chrono::milliseconds requestTimeout{10000}; // whole request
    std::chrono::milliseconds writeTimeout{10000};   // whole HTTP response
    std::chrono::milliseconds eventPollInterval{1000};
    std::uint32_t maxRequestsPerWindow = MAX_REQUESTS_PER_WINDOW;
    std::uint64_t rateLimitWindowSeconds = RATE_LIMIT_WINDOW_SECONDS;
  };

  // `runtimeMutex` must be the same mutex the owning NodeOrchestrator holds
  // while ticking, so RPC request handling and block production/consensus
  // never touch NodeRuntime (blockchain, mempool, ...) at the same time.
  NodeRpcServer(NodeRuntime &runtime, std::mutex &runtimeMutex,
                NodeEventBus *eventBus = nullptr,
                std::uint16_t port = DEFAULT_PORT,
                const std::string &bindAddr = "127.0.0.1");

  // Overload that also wires a GossipMesh for broadcasting submitted
  // transactions.
  NodeRpcServer(NodeRuntime &runtime, std::mutex &runtimeMutex,
                p2p::GossipMesh &gossip, NodeEventBus *eventBus,
                std::uint16_t port = DEFAULT_PORT,
                const std::string &bindAddr = "127.0.0.1");

  ~NodeRpcServer();

  NodeRpcServer(const NodeRpcServer &) = delete;
  NodeRpcServer &operator=(const NodeRpcServer &) = delete;

  // Binds and starts serving. Throws std::runtime_error if the bind address
  // cannot be resolved or bound. bindAddr may be an IPv4 or IPv6 literal or a
  // host name such as "localhost"; port 0 binds an ephemeral port, which
  // port() then reports.
  void start();
  // Closes every connection and joins the worker threads. Safe to call twice.
  void stop();
  bool isRunning() const;

  // Replaces the transport limits. Throws std::logic_error while running.
  void setLimits(const Limits &limits);
  const Limits &limits() const;

  // Optional operational health source owned by NodeOrchestrator. When absent,
  // metrics still expose runtime/RPC/event data and report sync as UNKNOWN.
  void attachSyncHealth(const SyncHealth *syncHealth);

  // Optional data directory owned by NodeOrchestrator; enables the storage,
  // checkpoint, pruning and archive methods and history metrics.
  void attachDataDirectory(const NodeDataDirectoryConfig *directory);

  std::uint16_t port() const;

private:
  struct Transport; // io_context, acceptor and worker threads
  class Session;    // one accepted connection: an HTTP request or a WebSocket

  NodeRuntime &m_runtime;
  std::mutex &m_runtimeMutex; // shared with the owning NodeOrchestrator
  p2p::GossipMesh *m_gossip;  // optional; nullptr means no gossip broadcast
  std::uint16_t m_configuredPort;    // what start() binds; 0 = ephemeral
  std::atomic<std::uint16_t> m_port; // the port actually bound
  std::string m_bindAddr;
  std::atomic<bool> m_running;
  Limits m_limits;
  std::unique_ptr<Transport> m_transport;
  std::atomic<std::size_t> m_activeConnections;
  std::atomic<std::size_t> m_activeWebSockets;
  NodeEventBus m_ownedEventBus;
  NodeEventBus *m_eventBus;
  p2p::PeerRateLimiter m_rateLimiter; // touched only by the accept handler
  JsonRpcDispatcher m_jsonRpcDispatcher;
  const SyncHealth *m_syncHealth;
  const NodeDataDirectoryConfig *m_dataDirectory = nullptr;

  struct HttpDispatchResponse {
    int statusCode;
    std::string body;
    std::string contentType;

    HttpDispatchResponse(int code, std::string responseBody,
                         std::string responseContentType = "application/json");
  };

  void acceptNext();

  HttpDispatchResponse dispatch(const std::string &method,
                                const std::string &path,
                                const std::string &body);

  // Wires JSON-RPC methods to the same runtime-safe handlers used by the
  // operational REST surface. Called once during construction.
  void registerJsonRpcMethods();

  std::string handleJsonRpc(const std::string &body);

  // --- route handlers ---
  std::string handleStatus() const;
  std::string handleBlock(const std::string &heightStr) const;
  std::string handleBlockByHash(const std::string &blockHash) const;
  std::string handleTx(const std::string &txId) const;
  std::string handleAccount(const std::string &address) const;
  std::string handleAccountProof(const std::string &address) const;
  std::string handleValidators() const;
  std::string handleStakeStatus(const std::string &validatorAddress) const;
  std::string handleStakePositions(const std::string &ownerAddress) const;
  std::string handleStakePosition(const std::string &positionId) const;
  std::string
  handleStakePendingUnbonding(const std::string &validatorAddress) const;
  std::string handleStakeValidator(const std::string &validatorAddress) const;
  std::string handleStakeAudit() const;
  std::string handleStakeMutationInfo(const std::string &operation) const;
  std::string handlePeers() const;
  std::string handleMempool() const;
  std::string handleEstimateFee(const std::string &urgency) const;
  std::string handleChainInfo() const;
  std::string handleJsonRpcMethods() const;
  std::string handleLightCheckpoint() const;
  std::string handleLightHeaders(const std::string &fromHeight,
                                 const std::string &maxHeaders) const;
  std::string handleLightAccountProof(const std::string &address) const;
  std::string handleLightTransactionProof(const std::string &txId) const;
  std::string handleEvents(const std::string &afterSequence,
                           const std::string &type,
                           const std::string &limit) const;
  std::string handleHealth() const;
  std::string handleMetrics() const;
  std::string handlePrometheusMetrics() const;
  std::string handleStorageStatus() const;
  std::string handleCheckpoint(const std::string &height) const;
  std::string handlePruningStatus() const;
  std::string handleArchiveStatus() const;
  std::string handleArchiveReplication() const;
  std::string handleGovernanceStatus() const;
  std::string handleGovernanceProposals() const;
  std::string handleGovernanceProposal(const std::string &proposalId) const;
  std::string handleGovernanceVotes(const std::string &proposalId) const;
  std::string handleGovernanceTally(const std::string &proposalId) const;
  std::string handleGovernanceDecision(const std::string &proposalId) const;
  std::string handleGovernanceExecution(const std::string &proposalId) const;
  std::string handleSubmit(const std::string &body);

  // --- HTTP helpers ---
  static std::string
  httpResponse(int statusCode, const std::string &body,
               const std::string &contentType = "application/json");

  static std::string jsonError(const std::string &message);

  static std::string pathSegment(const std::string &path, int index);
};

} // namespace nodo::node

#endif
