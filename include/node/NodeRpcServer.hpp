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
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nodo::node {

/*
 * NodeRpcServer is the node HTTP surface. JSON-RPC is the official public
 * protocol API and is exposed through POST /rpc. The older REST routes remain
 * available as operational/development endpoints for status, health, metrics,
 * diagnostics and backward-compatible integration tests.
 *
 * Transport: POSIX TCP sockets, single-threaded accept loop.
 * Protocol:  Plain HTTP/1.0 with JSON response bodies.
 * Auth:      None — bind to loopback by default.
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
  static constexpr std::uint32_t MAX_REQUESTS_PER_WINDOW = 3000;
  static constexpr std::uint64_t RATE_LIMIT_WINDOW_SECONDS = 60;

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

  void start();
  void stop();
  bool isRunning() const;

  // Optional operational health source owned by NodeOrchestrator. When absent,
  // metrics still expose runtime/RPC/event data and report sync as UNKNOWN.
  void attachSyncHealth(const SyncHealth *syncHealth);

  std::uint16_t port() const;

private:
  NodeRuntime &m_runtime;
  std::mutex &m_runtimeMutex; // shared with the owning NodeOrchestrator
  p2p::GossipMesh *m_gossip;  // optional; nullptr means no gossip broadcast
  std::uint16_t m_port;
  std::string m_bindAddr;
  std::atomic<bool> m_running;
  std::thread m_thread;
  std::atomic<int> m_serverFd;
  mutable std::mutex m_clientThreadsMutex;
  std::vector<std::thread> m_clientThreads;
  NodeEventBus m_ownedEventBus;
  NodeEventBus *m_eventBus;
  p2p::PeerRateLimiter m_rateLimiter;
  JsonRpcDispatcher m_jsonRpcDispatcher;
  const SyncHealth *m_syncHealth;

  struct HttpDispatchResponse {
    int statusCode;
    std::string body;
    std::string contentType;

    HttpDispatchResponse(int code, std::string responseBody,
                         std::string responseContentType = "application/json");
  };

  void runLoop();
  void handleClient(int clientFd);
  void joinClientThreads();
  bool isWebSocketUpgrade(const std::string &request,
                          const std::string &path) const;
  void handleWebSocket(int clientFd, const std::string &request,
                       const std::string &path);

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

  static bool parseRequestLine(const std::string &request,
                               std::string &outMethod, std::string &outPath,
                               std::string &outBody);

  static std::string pathSegment(const std::string &path, int index);
};

} // namespace nodo::node

#endif
