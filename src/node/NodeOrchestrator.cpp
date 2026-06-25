#include "node/NodeOrchestrator.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "crypto/Signer.hpp"
#include "node/BlockAnnounceHandler.hpp"
#include "node/BlockSyncHandler.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/PeerHandshakeAutoRegistrar.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace nodo::node {

// ---------------------------------------------------------------------------
// NodeOrchestratorConfig
// ---------------------------------------------------------------------------

NodeOrchestratorConfig::NodeOrchestratorConfig()
    : m_rpcPort(8545)
    , m_rpcBindAddr("127.0.0.1")
    , m_consensusTickMs(100)
    , m_maxBlockTransactions(500)
{}

NodeOrchestratorConfig::NodeOrchestratorConfig(
    config::GenesisConfig       genesisConfig,
    NodeDataDirectoryConfig     dataDirectory,
    p2p::PeerInfo               localPeer,
    std::string                 localValidatorAddress,
    std::uint16_t               rpcPort,
    std::string                 rpcBindAddr,
    std::int64_t                consensusTickMs,
    std::size_t                 maxBlockTransactions
)
    : m_genesisConfig(std::move(genesisConfig))
    , m_dataDirectory(std::move(dataDirectory))
    , m_localPeer(std::move(localPeer))
    , m_localValidatorAddress(std::move(localValidatorAddress))
    , m_rpcPort(rpcPort)
    , m_rpcBindAddr(std::move(rpcBindAddr))
    , m_consensusTickMs(consensusTickMs)
    , m_maxBlockTransactions(maxBlockTransactions)
{}

const config::GenesisConfig&   NodeOrchestratorConfig::genesisConfig()         const { return m_genesisConfig; }
const NodeDataDirectoryConfig& NodeOrchestratorConfig::dataDirectory()          const { return m_dataDirectory; }
const p2p::PeerInfo&           NodeOrchestratorConfig::localPeer()              const { return m_localPeer; }
const std::string&             NodeOrchestratorConfig::localValidatorAddress()  const { return m_localValidatorAddress; }
std::uint16_t                  NodeOrchestratorConfig::rpcPort()                const { return m_rpcPort; }
const std::string&             NodeOrchestratorConfig::rpcBindAddr()            const { return m_rpcBindAddr; }
std::int64_t                   NodeOrchestratorConfig::consensusTickMs()        const { return m_consensusTickMs; }
std::size_t                    NodeOrchestratorConfig::maxBlockTransactions()   const { return m_maxBlockTransactions; }

bool NodeOrchestratorConfig::isValid() const {
    return m_genesisConfig.isValid() &&
           m_localPeer.isValid() &&
           !m_rpcBindAddr.empty() &&
           m_consensusTickMs > 0 &&
           m_maxBlockTransactions > 0;
}

// ---------------------------------------------------------------------------
// Free helpers
// ---------------------------------------------------------------------------

std::string nodeOrchestratorStartStatusToString(NodeOrchestratorStartStatus status) {
    switch (status) {
        case NodeOrchestratorStartStatus::RUNNING:         return "RUNNING";
        case NodeOrchestratorStartStatus::ALREADY_RUNNING: return "ALREADY_RUNNING";
        case NodeOrchestratorStartStatus::INIT_FAILED:     return "INIT_FAILED";
        case NodeOrchestratorStartStatus::STATE_LOAD_FAILED: return "STATE_LOAD_FAILED";
        case NodeOrchestratorStartStatus::TRANSPORT_FAILED: return "TRANSPORT_FAILED";
        case NodeOrchestratorStartStatus::CONSENSUS_FAILED: return "CONSENSUS_FAILED";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// NodeOrchestrator
// ---------------------------------------------------------------------------

NodeOrchestrator::NodeOrchestrator(
    NodeOrchestratorConfig       config,
    const crypto::CryptoPolicy&  policy,
    const crypto::SignatureProvider& provider
)
    : m_config(std::move(config))
    , m_policy(policy)
    , m_provider(provider)
    , m_running(false)
{}

NodeOrchestrator::~NodeOrchestrator() {
    stop();
}

// ---- Lifecycle ------------------------------------------------------------

NodeOrchestratorStartResult NodeOrchestrator::start() {
    if (m_running.load()) {
        return {NodeOrchestratorStartStatus::ALREADY_RUNNING, "Already running."};
    }

    // Step 1: Initialize data directory or load existing state.
    auto initResult = initOrLoad();
    if (!initResult.running()) return initResult;

    // Step 2: Start TCP transport + gossip mesh.
    if (!startTransport()) {
        return {NodeOrchestratorStartStatus::TRANSPORT_FAILED,
                "Failed to bind TCP transport."};
    }

    // Step 3: Start consensus event loop (background thread).
    if (!startConsensus()) {
        return {NodeOrchestratorStartStatus::CONSENSUS_FAILED,
                "Failed to start consensus event loop."};
    }

    // Step 4: Start RPC server (background thread).
    startRpc(); // Non-fatal if RPC fails.

    m_running.store(true);
    return initResult;
}

void NodeOrchestrator::stop() {
    if (!m_running.exchange(false)) return;

    if (m_rpcServer)       m_rpcServer->stop();
    if (m_consensusLoop)   m_consensusLoop->stop();
    if (m_discoveryService) m_discoveryService->stop();
    if (m_tcpRuntime)      m_tcpRuntime->stop();
}

bool NodeOrchestrator::isRunning() const { return m_running.load(); }

void NodeOrchestrator::runBlocking(std::int64_t tickIntervalMs) {
    while (m_running.load()) {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        tick(static_cast<std::int64_t>(now));

        std::this_thread::sleep_for(std::chrono::milliseconds(tickIntervalMs));
    }
}

// ---- Single tick ----------------------------------------------------------

void NodeOrchestrator::tick(std::int64_t now) {
    if (!m_tcpRuntime || !m_runtime) return;

    // 1. Drive the gossip/TCP layer: receive inbound + flush outbound.
    m_tcpRuntime->tick(now);

    auto& gossip = m_tcpRuntime->gossipMesh();

    // 2. Auto-register new peers that sent PEER_HELLO.
    PeerHandshakeAutoRegistrar::processInbox(
        gossip,
        currentChainStatus(),
        now
    );

    // 3. Apply any incoming BLOCK_ANNOUNCE messages.
    BlockAnnounceHandler::processInbox(
        gossip,
        m_runtime->mutableBlockchain(),
        now
    );

    // 4. Serve incoming BLOCK_REQUEST messages.
    BlockSyncHandler::serveRequests(
        gossip,
        m_runtime->blockchain(),
        now
    );

    // 5. Apply any received BLOCK_RESPONSE messages.
    const std::uint64_t localHeight =
        m_runtime->blockchain().empty() ? 0
        : m_runtime->blockchain().latestBlock().index();

    BlockSyncHandler::applyResponses(
        gossip,
        m_runtime->mutableBlockchain(),
        now
    );

    // 6. If peer is ahead of us, request missing blocks.
    // Only trust peers that have completed handshake (non-empty peerId and endpoint).
    // Also guard against height-spoofing attacks by capping the look-ahead window.
    static constexpr std::uint64_t kMaxSyncLookAhead = 10000;
    const auto peers = m_runtime->peerManager().peers();
    for (const auto& peer : peers) {
        // Skip peers that have not completed the handshake.
        if (peer.peerId().empty() || peer.endpoint().empty()) continue;

        // Ignore implausibly large claimed heights (height-spoofing protection).
        if (peer.latestKnownHeight() > localHeight + kMaxSyncLookAhead) continue;

        if (peer.latestKnownHeight() > localHeight + 1) {
            BlockSyncHandler::requestBlocks(
                gossip,
                m_config.localPeer().peerId(),
                localHeight + 1,
                BlockSyncHandler::MAX_BLOCKS_PER_RESPONSE,
                now
            );
            break; // request from first ahead-peer; retries happen next tick
        }
    }
}

// ---- Accessors ------------------------------------------------------------

const NodeRuntime& NodeOrchestrator::runtime() const {
    if (!m_runtime) throw std::runtime_error("NodeOrchestrator: runtime not started");
    return *m_runtime;
}

NodeRuntime& NodeOrchestrator::mutableRuntime() {
    if (!m_runtime) throw std::runtime_error("NodeOrchestrator: runtime not started");
    return *m_runtime;
}

const TcpTestnetNodeRuntime& NodeOrchestrator::tcpRuntime() const {
    if (!m_tcpRuntime) throw std::runtime_error("NodeOrchestrator: transport not started");
    return *m_tcpRuntime;
}

const NodeOrchestratorConfig&  NodeOrchestrator::config()        const { return m_config; }
const consensus::EvidencePool& NodeOrchestrator::evidencePool()  const { return m_evidencePool; }

std::vector<p2p::NetworkEnvelope> NodeOrchestrator::drainGossipInbox(
    p2p::NetworkMessageType type
) {
    if (!m_tcpRuntime) return {};
    return m_tcpRuntime->gossipMesh().drainInbox(type);
}

p2p::GossipDeliveryReport NodeOrchestrator::gossipBroadcast(
    p2p::NetworkMessageType type,
    const std::string& payload,
    std::int64_t now
) {
    if (!m_tcpRuntime) return {};
    return m_tcpRuntime->gossipMesh().broadcast(type, payload, now);
}

void NodeOrchestrator::addAndConnectPeer(const p2p::PeerMetadata& peer) {
    if (!m_tcpRuntime) return;
    if (m_tcpRuntime->addPeer(peer).success()) {
        m_tcpRuntime->connectPeer(peer.nodeId());
    }
}

void NodeOrchestrator::triggerSyncIfBehind(
    const ChainStatusMessage& remotePeerStatus,
    std::int64_t now
) {
    if (!m_runtime || !m_tcpRuntime) return;

    PersistentSyncCheckpointStore store(
        m_config.dataDirectory().rootPath()
    );

    PersistentSyncCheckpoint checkpoint;
    const auto loaded = store.load();
    if (loaded.has_value()) {
        checkpoint = loaded.value();
    } else {
        const auto& chain = m_runtime->blockchain();
        const std::uint64_t height =
            chain.empty() ? 0 : chain.latestBlock().index();
        const std::string hash =
            chain.empty() ? "" : chain.latestBlock().hash();
        checkpoint = PersistentSyncCheckpoint::genesis(
            hash.empty() ? "genesis" : hash,
            "state-root-" + std::to_string(height),
            now
        );
    }

    const PersistentSyncPlan plan = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        remotePeerStatus,
        m_config.localPeer().peerId(),
        remotePeerStatus.chainId(),
        NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH,
        NODO_PERSISTENT_SYNC_DEFAULT_SNAPSHOT_THRESHOLD,
        now
    );

    if (!plan.requestBlocks()) return;

    const NetworkBlockSyncRequest& req = plan.blockRequest().value();
    m_tcpRuntime->gossipMesh().broadcast(
        p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
        req.serialize(),
        now
    );
}

// ---- Internal startup helpers ---------------------------------------------

NodeOrchestratorStartResult NodeOrchestrator::initOrLoad() {
    bool freshGenesis = false;

    if (!NodeDataDirectory::isInitialized(m_config.dataDirectory())) {
        // First boot: initialize from genesis.
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        const auto initResult = NodeDataDirectory::initialize(
            m_config.dataDirectory(),
            m_config.genesisConfig(),
            m_config.localPeer(),
            now
        );

        if (!initResult.initialized() && !initResult.alreadyInitialized()) {
            return {NodeOrchestratorStartStatus::INIT_FAILED,
                    "NodeDataDirectory::initialize failed"};
        }

        // Start runtime from genesis.
        NodeRuntimeConfig runtimeConfig(
            m_config.genesisConfig(),
            m_config.localPeer(),
            128
        );

        auto startResult = NodeRuntimeFactory::startFromGenesis(runtimeConfig);
        if (!startResult.started()) {
            return {NodeOrchestratorStartStatus::INIT_FAILED,
                    "NodeRuntimeFactory::startFromGenesis failed: " + startResult.reason()};
        }

        m_runtime = std::make_unique<NodeRuntime>(std::move(startResult.runtime()));
        freshGenesis = true;
    } else {
        // Subsequent boot: reload from disk.
        const auto loadResult = RuntimeStateLoader::loadFromDataDirectory(
            m_config.dataDirectory(),
            m_config.genesisConfig(),
            m_config.localPeer()
        );

        if (!loadResult.loaded()) {
            return {NodeOrchestratorStartStatus::STATE_LOAD_FAILED,
                    "RuntimeStateLoader failed: " + loadResult.reason()};
        }

        m_runtime = std::make_unique<NodeRuntime>(std::move(loadResult.runtime()));
    }

    NodeOrchestratorStartResult result;
    result.status       = NodeOrchestratorStartStatus::RUNNING;
    result.freshGenesis = freshGenesis;
    return result;
}

bool NodeOrchestrator::startTransport() {
    m_tcpRuntime = std::make_unique<TcpTestnetNodeRuntime>(
        buildTransportConfig()
    );

    const auto transportResult = m_tcpRuntime->start();
    if (!transportResult.success()) return false;

    // Extract TCP port to compute UDP port
    const auto& peer = m_config.localPeer();
    const std::string& endpoint = peer.endpoint();
    std::uint16_t tcpPort = 30333;
    const auto colon = endpoint.rfind(':');
    if (colon != std::string::npos) {
        try {
            tcpPort = static_cast<std::uint16_t>(
                std::stoul(endpoint.substr(colon + 1))
            );
        } catch (...) {}
    }
    std::uint16_t udpPort = tcpPort + 1;

    m_discoveryService = std::make_unique<p2p::DiscoveryService>(
        peer.peerId(),
        udpPort,
        tcpPort
    );

    m_discoveryService->registerPeerDiscoveredCallback(
        [this](const std::string& peerId, const std::string& host, std::uint16_t port) {
            if (m_tcpRuntime) {
                const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                p2p::PeerMetadata peerMeta(
                    peerId,
                    p2p::PeerEndpoint(host, port),
                    "",
                    nowSec,
                    nowSec,
                    0,
                    false
                );
                if (m_tcpRuntime->addPeer(peerMeta).success()) {
                    m_tcpRuntime->connectPeer(peerId);
                }
            }
        }
    );

    m_discoveryService->start();

    // Load known peers from disk, seed them into the discovery service, and connect.
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    m_tcpRuntime->loadPeersFromDisk(now);

    const auto entries = TcpTestnetPeerStore::load(m_tcpRuntime->config().peersFilePath());
    for (const auto& entry : entries) {
        m_discoveryService->addPeer(
            entry.nodeId(),
            entry.endpoint().host(),
            entry.endpoint().port(),
            entry.endpoint().port() + 1
        );
    }

    return true;
}

bool NodeOrchestrator::startConsensus() {
    m_consensusLoop = std::make_unique<consensus::ConsensusEventLoop>(
        *m_runtime,
        m_tcpRuntime->gossipMesh(),
        m_policy,
        m_provider
    );

    // Wire the local validator address so the loop knows when to propose.
    m_consensusLoop->setLocalValidatorAddress(
        m_config.localValidatorAddress()
    );

    if (m_localSigner.has_value()) {
        m_consensusLoop->setLocalSigner(&m_localSigner.value());
    }

    // Wire the consensus recovery path for BFT lock/vote persistence.
    const std::filesystem::path recoveryPath =
        m_config.dataDirectory().consensusRecoveryPath();
    m_consensusLoop->setRecoveryPath(recoveryPath);

    // Restore lock/vote state from disk so we don't double-vote after restart.
    const auto recoveryState =
        consensus::ConsensusRecoveryStore::load(recoveryPath);
    if (recoveryState.has_value()) {
        m_consensusLoop->loadFromRecoveryState(*recoveryState);
    }

    // Wire the block producer callback.
    m_consensusLoop->setBlockProducerCallback(
        [this](std::uint64_t height, std::uint64_t round, std::int64_t now) -> bool {
            return this->produceBlock(height, round, now);
        }
    );

    // Wire the evidence pool for double-vote detection.
    m_consensusLoop->setEvidencePool(&m_evidencePool);

    // Wire a finalized-block callback that broadcasts the block to peers.
    m_consensusLoop->setFinalizedCallback(
        [this](const consensus::FinalizedBlockRecord& rec) {
            if (!m_tcpRuntime || m_runtime->blockchain().empty()) return;

            const auto& blocks = m_runtime->blockchain().blocks();
            if (rec.blockIndex() >= blocks.size()) return;

            const core::Block& block = blocks[static_cast<std::size_t>(rec.blockIndex())];
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            BlockAnnounceHandler::broadcastBlock(
                block,
                m_tcpRuntime->gossipMesh(),
                now
            );
        }
    );

    m_consensusLoop->start(m_config.consensusTickMs());
    return true;
}

bool NodeOrchestrator::startRpc() {
    try {
        m_rpcServer = std::make_unique<NodeRpcServer>(
            *m_runtime,
            m_config.rpcPort(),
            m_config.rpcBindAddr()
        );
        m_rpcServer->start();
        return true;
    } catch (...) {
        // RPC failure is non-fatal — node still participates in consensus.
        m_rpcServer.reset();
        return false;
    }
}

void NodeOrchestrator::setLocalSigner(crypto::Signer signer) {
    m_localSigner = std::move(signer);
}

bool NodeOrchestrator::produceBlock(
    std::uint64_t height,
    std::uint64_t round,
    std::int64_t  now
) {
    // For now: if the runtime has no validators, skip.
    if (m_runtime->validatorRegistry().activeCount() == 0) return false;

    // Check if we're proposer for this height+round.
    const std::string chainId =
        m_config.genesisConfig().networkParameters().chainId();
    const std::string expectedProposer = consensus::ProposerSchedule::selectProposer(
        m_runtime->validatorRegistry(),
        chainId,
        height,
        round
    );

    if (expectedProposer != m_config.localValidatorAddress()) return false;

    if (!m_localSigner.has_value()) return false;

    RuntimeBlockPipelineConfig pipelineConfig(
        m_config.maxBlockTransactions(),
        0,
        round,
        now
    );

    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        *m_runtime,
        pipelineConfig,
        m_localSigner.value()
    );
    return result.status() == RuntimeBlockPipelineStatus::FINALIZED;
}

ChainStatusMessage NodeOrchestrator::currentChainStatus() const {
    if (!m_runtime) {
        return ChainStatusMessage{};
    }

    const auto& chain = m_runtime->blockchain();
    const std::uint64_t height =
        chain.empty() ? 0 : chain.latestBlock().index();
    const std::string& latestHash =
        chain.empty() ? "" : chain.latestBlock().hash();

    const auto& finReg = m_runtime->finalizationRegistry();
    const std::uint64_t finalizedHeight = finReg.highestFinalizedHeight();
    const std::string finalizedHash =
        finReg.recordForHeight(finalizedHeight)
            ? finReg.recordForHeight(finalizedHeight)->blockHash()
            : latestHash;

    const std::string chainId =
        m_config.genesisConfig().networkParameters().chainId();
    const std::string networkName =
        m_config.genesisConfig().networkParameters().networkName();

    return ChainStatusMessage(
        networkName,
        chainId,
        m_config.localPeer().protocolVersion(),
        height,
        latestHash,
        finalizedHeight,
        finalizedHash
    );
}

TcpTestnetNodeRuntimeConfig NodeOrchestrator::buildTransportConfig() const {
    const auto& genesis = m_config.genesisConfig();
    const auto& np      = genesis.networkParameters();
    const auto& peer    = m_config.localPeer();

    // Parse host:port from peer endpoint.
    const std::string& endpoint = peer.endpoint();
    std::string host = "127.0.0.1";
    std::uint16_t port = 30333;

    const auto colon = endpoint.rfind(':');
    if (colon != std::string::npos) {
        host = endpoint.substr(0, colon);
        try {
            port = static_cast<std::uint16_t>(
                std::stoul(endpoint.substr(colon + 1))
            );
        } catch (...) {}
    }

    return TcpTestnetNodeRuntimeConfig(
        peer.peerId(),
        host,
        port,
        np.networkName(),
        np.chainId(),
        peer.protocolVersion(),
        genesis.deterministicId(),
        m_config.dataDirectory().rootPath(),
        30,    // defaultTtlSeconds
        10     // invalidMessageQuarantineThreshold
    );
}

} // namespace nodo::node
