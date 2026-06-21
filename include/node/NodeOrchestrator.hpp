#ifndef NODO_NODE_NODE_ORCHESTRATOR_HPP
#define NODO_NODE_NODE_ORCHESTRATOR_HPP

#include "config/NetworkParameters.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/EvidencePool.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRpcServer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/TcpTestnetNodeRuntime.hpp"
#include "p2p/Peer.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

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

    NodeOrchestratorConfig(
        config::GenesisConfig       genesisConfig,
        NodeDataDirectoryConfig     dataDirectory,
        p2p::PeerInfo               localPeer,
        std::string                 localValidatorAddress,
        std::uint16_t               rpcPort,
        std::string                 rpcBindAddr,
        std::int64_t                consensusTickMs,
        std::size_t                 maxBlockTransactions
    );

    const config::GenesisConfig&    genesisConfig()          const;
    const NodeDataDirectoryConfig&  dataDirectory()          const;
    const p2p::PeerInfo&            localPeer()              const;
    const std::string&              localValidatorAddress()  const;
    std::uint16_t                   rpcPort()                const;
    const std::string&              rpcBindAddr()            const;
    std::int64_t                    consensusTickMs()        const;
    std::size_t                     maxBlockTransactions()   const;

    bool isValid() const;

private:
    config::GenesisConfig       m_genesisConfig;
    NodeDataDirectoryConfig     m_dataDirectory;
    p2p::PeerInfo               m_localPeer;
    std::string                 m_localValidatorAddress;
    std::uint16_t               m_rpcPort;
    std::string                 m_rpcBindAddr;
    std::int64_t                m_consensusTickMs;
    std::size_t                 m_maxBlockTransactions;
};

enum class NodeOrchestratorStartStatus {
    RUNNING,
    ALREADY_RUNNING,
    INIT_FAILED,
    STATE_LOAD_FAILED,
    TRANSPORT_FAILED,
    CONSENSUS_FAILED
};

std::string nodeOrchestratorStartStatusToString(NodeOrchestratorStartStatus status);

struct NodeOrchestratorStartResult {
    NodeOrchestratorStartStatus status;
    std::string                 reason;
    bool                        freshGenesis = false;

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
 *   - The gossip/network tick is driven by the caller's thread or runBlocking().
 */
class NodeOrchestrator {
public:
    static constexpr std::int64_t DEFAULT_TICK_INTERVAL_MS = 50;

    explicit NodeOrchestrator(
        NodeOrchestratorConfig      config,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

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

    // ---- Single tick (usable from tests without a background thread) ------

    /*
     * Execute one network tick:
     *   - gossip receive/flush
     *   - peer handshake registration
     *   - block announce processing
     *   - block sync (serve requests + apply responses)
     */
    void tick(std::int64_t now);

    // ---- Accessors (read-only for monitoring / tests) ---------------------

    const NodeRuntime&              runtime()        const;
    const TcpTestnetNodeRuntime&    tcpRuntime()     const;
    const NodeOrchestratorConfig&   config()         const;
    const consensus::EvidencePool&  evidencePool()   const;

private:
    NodeOrchestratorConfig               m_config;
    const crypto::CryptoPolicy&          m_policy;
    const crypto::SignatureProvider&     m_provider;

    // Core state (populated in start())
    std::unique_ptr<NodeRuntime>             m_runtime;
    std::unique_ptr<TcpTestnetNodeRuntime>   m_tcpRuntime;
    std::unique_ptr<consensus::ConsensusEventLoop> m_consensusLoop;
    std::unique_ptr<NodeRpcServer>           m_rpcServer;
    consensus::EvidencePool                  m_evidencePool;

    std::atomic<bool> m_running;

    // ---- Internal startup helpers ----------------------------------------

    NodeOrchestratorStartResult initOrLoad();
    bool startTransport();
    bool startConsensus();
    bool startRpc();

    // Block producer callback wired into ConsensusEventLoop.
    bool produceBlock(std::uint64_t height, std::uint64_t round, std::int64_t now);

    // Build chain status from current runtime state.
    ChainStatusMessage currentChainStatus() const;

    TcpTestnetNodeRuntimeConfig buildTransportConfig() const;
};

} // namespace nodo::node

#endif
