#ifndef NODO_NODE_NODE_RPC_SERVER_HPP
#define NODO_NODE_NODE_RPC_SERVER_HPP

#include "node/NodeRuntime.hpp"
#include "p2p/GossipMesh.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace nodo::node {

/*
 * NodeRpcServer is a minimal HTTP/JSON server that exposes node state for
 * operators, dashboards and integration tests.
 *
 * Transport: POSIX TCP sockets, single-threaded accept loop.
 * Protocol:  Plain HTTP/1.0 with JSON response bodies.
 * Auth:      None — bind to loopback by default.
 *
 * Endpoints (all read-only except /submit):
 *   GET  /status               — node height, round, peer count, mempool size
 *   GET  /block/{height}       — serialized block at given height
 *   GET  /tx/{txId}            — ledger record with matching id or sourceId
 *   GET  /account/{address}    — balance and nonce for address
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
    static constexpr std::uint16_t DEFAULT_PORT    = 8545;
    static constexpr std::size_t   MAX_REQUEST_LEN = 65536;

    NodeRpcServer(
        NodeRuntime&       runtime,
        std::uint16_t      port    = DEFAULT_PORT,
        const std::string& bindAddr = "127.0.0.1"
    );

    // Overload that also wires a GossipMesh for broadcasting submitted transactions.
    NodeRpcServer(
        NodeRuntime&       runtime,
        p2p::GossipMesh&   gossip,
        std::uint16_t      port    = DEFAULT_PORT,
        const std::string& bindAddr = "127.0.0.1"
    );

    ~NodeRpcServer();

    void start();
    void stop();
    bool isRunning() const;

    std::uint16_t port() const;

private:
    NodeRuntime&       m_runtime;
    p2p::GossipMesh*   m_gossip;   // optional; nullptr means no gossip broadcast
    std::uint16_t      m_port;
    std::string        m_bindAddr;
    std::atomic<bool>  m_running;
    std::thread        m_thread;
    std::atomic<int>   m_serverFd;

    void runLoop();
    void handleClient(int clientFd);

    // Returns (statusCode, responseBody).
    std::pair<int, std::string> dispatch(
        const std::string& method,
        const std::string& path,
        const std::string& body
    );

    // --- route handlers ---
    std::string handleStatus() const;
    std::string handleBlock(const std::string& heightStr) const;
    std::string handleTx(const std::string& txId) const;
    std::string handleAccount(const std::string& address) const;
    std::string handleValidators() const;
    std::string handleStakeStatus(const std::string& validatorAddress) const;
    std::string handleStakePositions(const std::string& ownerAddress) const;
    std::string handleStakePosition(const std::string& positionId) const;
    std::string handleStakePendingUnbonding(const std::string& validatorAddress) const;
    std::string handleStakeValidator(const std::string& validatorAddress) const;
    std::string handleStakeAudit() const;
    std::string handleStakeMutationInfo(const std::string& operation) const;
    std::string handlePeers() const;
    std::string handleMempool() const;
    std::string handleGovernanceStatus() const;
    std::string handleGovernanceProposals() const;
    std::string handleGovernanceProposal(const std::string& proposalId) const;
    std::string handleGovernanceVotes(const std::string& proposalId) const;
    std::string handleGovernanceTally(const std::string& proposalId) const;
    std::string handleGovernanceDecision(const std::string& proposalId) const;
    std::string handleGovernanceExecution(const std::string& proposalId) const;
    std::string handleSubmit(const std::string& body);

    // --- HTTP helpers ---
    static std::string httpResponse(
        int statusCode,
        const std::string& body
    );

    static std::string jsonError(const std::string& message);

    static bool parseRequestLine(
        const std::string& request,
        std::string& outMethod,
        std::string& outPath,
        std::string& outBody
    );

    static std::string pathSegment(const std::string& path, int index);
};

} // namespace nodo::node

#endif
