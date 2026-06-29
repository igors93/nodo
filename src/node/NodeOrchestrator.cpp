#include "node/NodeOrchestrator.hpp"

#include "node/CanonicalSlashingTransition.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "crypto/Hex.hpp"
#include "crypto/Signer.hpp"
#include "node/BlockAnnounceHandler.hpp"
#include "node/ChainStatusGossipCodec.hpp"
#include "node/SyncHealth.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/PeerHandshakeAutoRegistrar.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/PersistentSlashingEvidencePool.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "serialization/ProtocolMessageCodec.hpp"
#include "storage/AccountStateSnapshotStore.hpp"
#include "storage/SlashingEvidenceStore.hpp"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <limits>
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

const config::GenesisConfig&   NodeOrchestratorConfig::genesisConfig()          const { return m_genesisConfig; }
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
// Internal persistence helper (file-scope)
// ---------------------------------------------------------------------------

namespace {

static const std::string kOrchestratorCanonicalPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

// Decode a canonical-hex gossip payload; returns std::nullopt if not in
// canonical format or if hex decoding fails.
std::optional<std::vector<unsigned char>> tryUnwrapCanonical(
    const std::string& payload
) {
    if (payload.rfind(kOrchestratorCanonicalPrefix, 0) != 0) {
        return std::nullopt;
    }
    const std::string hex = payload.substr(kOrchestratorCanonicalPrefix.size());
    if (!crypto::isHexString(hex)) return std::nullopt;
    try {
        return crypto::hexDecode(hex);
    } catch (...) {
        return std::nullopt;
    }
}

// Build a PersistentBlockSyncBatch from [fromHeight, fromHeight+maxItems) of the
// local chain. When a durable QC record exists for a block, it is embedded in
// the batch item so the receiver can perform fast-path QC-required sync without
// needing to recompute the state root from scratch.
PersistentBlockSyncBatch buildSyncResponseBatch(
    const core::Blockchain& blockchain,
    const NodeDataDirectoryConfig& directoryConfig,
    const std::string& localPeerId,
    std::uint64_t fromHeight,
    std::uint64_t maxItems,
    std::int64_t now
) {
    std::vector<PersistentBlockSyncItem> items;
    items.reserve(static_cast<std::size_t>(maxItems));
    for (const auto& block : blockchain.blocks()) {
        if (block.index() < fromHeight) continue;
        if (items.size() >= static_cast<std::size_t>(maxItems)) break;

        const FinalizedBlockArtifact artifact =
            FinalizedBlockArtifactCodec::readBlockArtifactFile(
                FinalizedBlockStore::blockFilePath(directoryConfig, block.index())
            );
        if (artifact.block().hash() != block.hash()) {
            throw std::runtime_error("Canonical sync artifact does not match runtime chain.");
        }

        items.emplace_back(
            block.index(),
            block.hash(),
            block.previousHash(),
            block.serialize(),
            block.hasCanonicalStateRoot()
                ? block.stateRoot()
                : "height-" + std::to_string(block.index()),
            now,
            artifact.finalizedRecord().serialize()
        );
    }
    if (items.empty()) return PersistentBlockSyncBatch{};
    const std::uint64_t firstHeight = items.front().height();
    const std::uint64_t lastHeight = items.back().height();
    return PersistentBlockSyncBatch(
        localPeerId,
        firstHeight,
        lastHeight,
        std::move(items),
        now
    );
}

} // namespace

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

    // Initialize data directory or load existing state.
    auto initResult = initOrLoad();
    if (!initResult.running()) return initResult;

    // Start TCP transport + gossip mesh.
    try {
        if (!startTransport()) {
            stop();
            return {NodeOrchestratorStartStatus::TRANSPORT_FAILED,
                    "Failed to bind TCP transport."};
        }
    } catch (const std::exception& error) {
        stop();
        return {NodeOrchestratorStartStatus::TRANSPORT_FAILED,
                std::string("TCP transport startup failed: ") + error.what()};
    }

    // Start consensus event loop (background thread).
    try {
        if (!startConsensus()) {
            stop();
            return {NodeOrchestratorStartStatus::CONSENSUS_FAILED,
                    "Failed to start consensus event loop."};
        }
    } catch (const std::exception& error) {
        stop();
        return {NodeOrchestratorStartStatus::CONSENSUS_FAILED,
                std::string("Consensus startup failed: ") + error.what()};
    }

    // Start RPC server (background thread).
    startRpc(); // Non-fatal if RPC fails.

    m_running.store(true);
    return initResult;
}

void NodeOrchestrator::stop() {
    m_running.store(false);

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

    // Drive the gossip/TCP layer: receive inbound + flush outbound.
    m_tcpRuntime->tick(now);

    auto& gossip = m_tcpRuntime->gossipMesh();

    // Snapshot our chain height once. Used throughout this tick.
    const std::uint64_t localHeight =
        m_runtime->blockchain().empty() ? 0
        : m_runtime->blockchain().latestBlock().index();

    // Drive challenge/response authentication before other peer messages.
    if (m_localNodeIdentity.has_value()) {
        const auto localPeer = localHandshakePeer(now);
        if (localPeer.has_value()) {
            PeerHandshakeAutoRegistrar::processInbox(
                gossip,
                *localPeer,
                currentChainStatus(),
                *m_localNodeIdentity,
                now
            );
        }
    }

    // Drain incoming CHAIN_STATUS messages.
    // These are broadcast by peers after round advances or on handshake. We use
    // them to keep our local peer-height registry accurate and to trigger
    // persistent sync when a peer is materially ahead of us.
    {
        const auto chainStatusMsgs = gossip.drainInbox(
            p2p::NetworkMessageType::CHAIN_STATUS
        );
        for (const auto& envelope : chainStatusMsgs) {
            const std::optional<ChainStatusMessage> statusMessage =
                ChainStatusGossipCodec::decode(envelope.payload());
            if (!statusMessage.has_value()) continue;
            try {
                const ChainStatusMessage& status = statusMessage.value();
                const auto& localParameters =
                    m_config.genesisConfig().networkParameters();
                if (status.chainId() != localParameters.chainId() ||
                    status.networkId() != localParameters.networkName()) {
                    continue;
                }

                // Update peer height so step 6 (block request) stays accurate.
                const p2p::PeerInfo* existing =
                    m_runtime->peerManager().peer(envelope.senderNodeId());
                if (existing != nullptr &&
                    status.latestHeight() > existing->latestKnownHeight()) {
                    m_runtime->mutablePeerManager().addOrUpdatePeer(p2p::PeerInfo(
                        existing->peerId(),
                        existing->endpoint(),
                        existing->protocolVersion(),
                        status.latestHeight(),
                        now
                    ));
                }

                // Trigger persistent sync if peer is ahead by more than one block.
                if (status.peerIsAheadOf(localHeight)) {
                    triggerSyncIfBehind(status, envelope.senderNodeId(), now);
                }
            } catch (const std::exception& e) {
                m_syncHealth.recordRequestFailure(
                    std::string("CHAIN_STATUS processing error: ") + e.what(), now
                );
            } catch (...) {
                m_syncHealth.recordRequestFailure("CHAIN_STATUS unknown error.", now);
            }
        }
    }

    // Announcements are hints, not finality proofs. Consensus proposals use
    // SIGNED_BLOCK_PROPOSAL; finalized catch-up uses BLOCK_SYNC_RESPONSE.
    // Never append an announced block directly to the canonical chain.
    gossip.drainInbox(p2p::NetworkMessageType::BLOCK_ANNOUNCE);

    // The legacy block-only protocol cannot reproduce full runtime state.
    gossip.drainInbox(p2p::NetworkMessageType::BLOCK_REQUEST);

    // Serve incoming BLOCK_SYNC_REQUEST messages (persistent sync path).
    // Decode each request, build a PersistentBlockSyncBatch from local blocks,
    // and broadcast a BLOCK_SYNC_RESPONSE so the requester can apply the batch.
    {
        const auto syncRequests = gossip.drainInbox(
            p2p::NetworkMessageType::BLOCK_SYNC_REQUEST
        );
        for (const auto& envelope : syncRequests) {
            const auto optBytes = tryUnwrapCanonical(envelope.payload());
            if (!optBytes.has_value()) continue;
            try {
                const NetworkBlockSyncRequest req =
                    serialization::ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
                        optBytes.value()
                    );
                if (!req.isValid()) continue;

                if (req.requesterNodeId() != envelope.senderNodeId()) {
                    continue;
                }

                if (req.locator().fromHeight() == 0 ||
                    req.locator().fromHeight() > m_runtime->blockchain().size()) {
                    continue;
                }
                const std::string& expectedAncestor =
                    m_runtime->blockchain().blocks()[
                        static_cast<std::size_t>(req.locator().fromHeight() - 1)
                    ].hash();
                if (std::find(
                        req.locator().knownAncestorHashes().begin(),
                        req.locator().knownAncestorHashes().end(),
                        expectedAncestor
                    ) == req.locator().knownAncestorHashes().end()) {
                    continue;
                }

                const std::uint64_t maxItems = std::min(
                    req.locator().maxBlocks(),
                    NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH
                );
                const PersistentBlockSyncBatch batch = buildSyncResponseBatch(
                    m_runtime->blockchain(),
                    m_config.dataDirectory(),
                    m_config.localPeer().peerId(),
                    req.locator().fromHeight(),
                    maxItems,
                    now
                );
                if (!batch.isValid()) continue;

                const std::vector<unsigned char> encoded =
                    PersistentBlockStateSyncCodec::encodeBlockSyncBatch(batch);
                gossip.sendTo(
                    req.requesterNodeId(),
                    p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE,
                    kOrchestratorCanonicalPrefix + crypto::hexEncode(encoded),
                    now
                );
            } catch (const std::exception& e) {
                m_syncHealth.recordServeFailure(
                    std::string("BLOCK_SYNC_REQUEST serve error: ") + e.what(), now
                );
            } catch (...) {
                m_syncHealth.recordServeFailure("BLOCK_SYNC_REQUEST unknown serve error.", now);
            }
        }
    }

    // Legacy BLOCK_RESPONSE messages do not carry the complete finalized
    // artifact needed to replay supply, governance and historical validator
    // state. Drain them; canonical synchronization uses BLOCK_SYNC_RESPONSE.
    gossip.drainInbox(p2p::NetworkMessageType::BLOCK_RESPONSE);

    // Apply any received BLOCK_SYNC_RESPONSE batches (persistent sync path).
    // Each batch is validated block-by-block with ProtocolCommitment mode before
    // being added to the chain. Applied blocks are persisted immediately.
    {
        const auto syncResponses = gossip.drainInbox(
            p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE
        );

        PersistentSyncCheckpointStore cpStore(m_config.dataDirectory().rootPath());
        const PersistentSyncCheckpointReadResult readCp = cpStore.read();
        const core::Block& localTip = m_runtime->blockchain().latestBlock();
        const bool checkpointMatchesRuntime =
            readCp.loaded() &&
            readCp.checkpoint().finalizedHeight() == localTip.index() &&
            readCp.checkpoint().finalizedBlockHash() == localTip.hash();
        PersistentSyncCheckpoint currentCheckpoint = checkpointMatchesRuntime
            ? readCp.checkpoint()
            : PersistentSyncCheckpoint(
                PersistentSyncCheckpoint::SCHEMA_VERSION,
                localTip.index(),
                localTip.hash(),
                localTip.hasCanonicalStateRoot()
                    ? localTip.stateRoot()
                    : "genesis-state-root",
                PersistentSyncStatus::IDLE,
                m_config.localPeer().peerId(),
                now
              );

        for (const auto& envelope : syncResponses) {
            const auto optBytes = tryUnwrapCanonical(envelope.payload());
            if (!optBytes.has_value()) continue;
            try {
                const PersistentBlockSyncBatch batch =
                    PersistentBlockStateSyncCodec::decodeBlockSyncBatch(
                        optBytes.value()
                    );

                if (!batch.isValid()) continue;

                if (batch.sourcePeerId() != envelope.senderNodeId()) {
                    continue;
                }

                // Checkpoint is saved atomically inside importFinalizedBatch,
                // before the in-memory runtime is mutated. No separate save needed.
                const PersistentSyncApplyResult applyResult =
                    PersistentBlockStateSyncApplier::importFinalizedBatch(
                        currentCheckpoint,
                        batch,
                        *m_runtime,
                        m_config.dataDirectory(),
                        &cpStore,
                        now
                    );

                if (applyResult.applied()) {
                    currentCheckpoint = applyResult.checkpoint().value();
                    m_syncHealth.recordSuccess();
                } else {
                    m_syncHealth.recordBatchFailure(applyResult.reason(), now);
                }
            } catch (const std::exception& e) {
                m_syncHealth.recordBatchFailure(
                    std::string("BLOCK_SYNC_RESPONSE import error: ") + e.what(), now
                );
            } catch (...) {
                m_syncHealth.recordBatchFailure("BLOCK_SYNC_RESPONSE unknown import error.", now);
            }
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

const NodeOrchestratorConfig&        NodeOrchestrator::config()              const { return m_config; }
const consensus::EvidencePool&       NodeOrchestrator::evidencePool()        const { return m_evidencePool; }
const crypto::CryptoPolicy&          NodeOrchestrator::cryptoPolicy()        const { return m_policy; }
const crypto::SignatureProvider&     NodeOrchestrator::signatureProvider()   const { return m_provider; }
const SyncHealth&                    NodeOrchestrator::syncHealth()          const { return m_syncHealth; }

bool NodeOrchestrator::rpcRunning() const {
    return m_rpcServer != nullptr && m_rpcServer->isRunning();
}

const std::string& NodeOrchestrator::rpcStartError() const {
    return m_rpcStartError;
}

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

void NodeOrchestrator::addAndConnectPeer(
    const p2p::PeerMetadata& peer,
    std::int64_t now
) {
    if (!m_tcpRuntime || !m_localNodeIdentity.has_value() || now <= 0) {
        return;
    }

    const p2p::TransportResult connected =
        m_tcpRuntime->connectUnverifiedPeer(
            peer.nodeId(),
            peer.endpoint()
        );
    if (!connected.success()) return;

    const auto localPeer = localHandshakePeer(now);
    if (localPeer.has_value()) {
        PeerHandshakeAutoRegistrar::initiateHandshake(
            m_tcpRuntime->gossipMesh(),
            peer.nodeId(),
            now
        );
    }
}

void NodeOrchestrator::triggerSyncIfBehind(
    const ChainStatusMessage& remotePeerStatus,
    const std::string& remotePeerId,
    std::int64_t now
) {
    if (!m_runtime || !m_tcpRuntime || remotePeerId.empty()) return;

    PersistentSyncCheckpointStore store(
        m_config.dataDirectory().rootPath()
    );

    PersistentSyncCheckpoint checkpoint;
    const PersistentSyncCheckpointReadResult readResult = store.read();
    const auto& chain = m_runtime->blockchain();
    const core::Block& tip = chain.latestBlock();
    if (readResult.loaded() &&
        readResult.checkpoint().finalizedHeight() == tip.index() &&
        readResult.checkpoint().finalizedBlockHash() == tip.hash()) {
        checkpoint = readResult.checkpoint();
    } else {
        checkpoint = PersistentSyncCheckpoint(
            PersistentSyncCheckpoint::SCHEMA_VERSION,
            tip.index(),
            tip.hash(),
            tip.hasCanonicalStateRoot() ? tip.stateRoot() : "genesis-state-root",
            PersistentSyncStatus::IDLE,
            m_config.localPeer().peerId(),
            now
        );
    }

    const PersistentSyncPlan plan = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        remotePeerStatus,
        m_config.localPeer().peerId(),
        remotePeerId,
        NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH,
        now
    );

    if (plan.requestSnapshot()) {
        // When the height gap is large, signal the peer that we need a snapshot.
        // The peer will respond with a SNAPSHOT_SYNC_RESPONSE (future message type).
        // For now we record the need and fall through — the peer may still serve
        // blocks if snapshot responses are not yet supported on its side.
        m_syncHealth.recordRequestFailure(
            "Snapshot requested but not yet supported; will retry with block sync.", now
        );
        return;
    }

    if (!plan.requestBlocks()) return;

    const NetworkBlockSyncRequest& req = plan.blockRequest().value();
    // Use canonical hex encoding so the receiving node can decode with
    // ProtocolMessageCodec::decodeNetworkBlockSyncRequest (step 4.5 in tick()).
    const std::string payload = kOrchestratorCanonicalPrefix +
        crypto::hexEncode(
            serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(req)
        );
    m_tcpRuntime->gossipMesh().sendTo(
        remotePeerId,
        p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
        payload,
        now
    );
}

// ---- Internal startup helpers ---------------------------------------------

NodeOrchestratorStartResult NodeOrchestrator::initOrLoad() {
    m_evidencePool = consensus::EvidencePool();
    m_slashingEvidenceStore.reset();
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

    m_slashingEvidenceStore =
        std::make_unique<storage::SlashingEvidenceStore>(
            m_config.dataDirectory().pendingSlashingEvidenceDirectoryPath()
        );
    const auto evidenceRestoreNow =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    const PersistentSlashingEvidencePoolLoadResult evidenceRestore =
        PersistentSlashingEvidencePool::restore(
            m_evidencePool,
            *m_slashingEvidenceStore,
            m_runtime->consensusRoundManager().currentState().height(),
            static_cast<std::int64_t>(evidenceRestoreNow),
            m_runtime->validatorSetHistory(),
            m_policy,
            m_provider,
            m_runtime->validatorPenaltyLedger()
        );
    if (!evidenceRestore.success()) {
        m_evidencePool = consensus::EvidencePool();
        m_slashingEvidenceStore.reset();
        return {
            NodeOrchestratorStartStatus::STATE_LOAD_FAILED,
            "Pending slashing evidence restore failed: " +
                evidenceRestore.reason()
        };
    }

    NodeOrchestratorStartResult result;
    result.status       = NodeOrchestratorStartStatus::RUNNING;
    result.freshGenesis = freshGenesis;
    return result;
}

bool NodeOrchestrator::startTransport() {
    const TcpTestnetNodeRuntimeConfig transportConfig = buildTransportConfig();
    if (transportConfig.port() == std::numeric_limits<std::uint16_t>::max()) {
        // Discovery uses the TCP successor port; 65535 has no valid successor.
        return false;
    }

    m_tcpRuntime = std::make_unique<TcpTestnetNodeRuntime>(
        transportConfig
    );

    const auto transportResult = m_tcpRuntime->start();
    if (!transportResult.success()) return false;

    // Discovery uses the port immediately after the validated TCP port.
    const auto& peer = m_config.localPeer();
    const std::uint16_t tcpPort = transportConfig.port();
    const std::uint16_t udpPort = static_cast<std::uint16_t>(tcpPort + 1);

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
                addAndConnectPeer(peerMeta, nowSec);
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
        if (entry.hasPersistentState() && entry.quarantined()) {
            continue;
        }
        if (entry.endpoint().port() ==
            std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }
        m_discoveryService->addPeer(
            entry.nodeId(),
            entry.endpoint().host(),
            entry.endpoint().port(),
            static_cast<std::uint16_t>(entry.endpoint().port() + 1)
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
    m_consensusLoop->setDataDirectoryConfig(&m_config.dataDirectory());

    // Restore lock/vote state from disk so we don't double-vote after restart.
    const auto storedRecoveryState =
        consensus::ConsensusRecoveryStore::load(recoveryPath);
    if (!storedRecoveryState.has_value()) {
        return false;
    }

    consensus::ConsensusRoundState recoveryState =
        storedRecoveryState.value();
    const consensus::ConsensusRoundState& currentState =
        m_runtime->consensusRoundManager().currentState();

    if (recoveryState.height() < currentState.height()) {
        if (!m_runtime->finalizationRegistry().hasFinalizedHeight(
                recoveryState.height()) ||
            !consensus::ConsensusRecoveryStore::save(
                recoveryPath, currentState)) {
            return false;
        }
        recoveryState = currentState;
    }

    if (recoveryState.height() != currentState.height() ||
        recoveryState.round() != currentState.round() ||
        recoveryState.proposerAddress() != currentState.proposerAddress() ||
        recoveryState.roundStartedAt() != currentState.roundStartedAt() ||
        !m_runtime->validatorSetHistory().hasSet(recoveryState.height())) {
        return false;
    }

    const std::string expectedProposer =
        consensus::ProposerSchedule::selectProposer(
            m_runtime->validatorSetHistory().setAt(recoveryState.height()),
            m_config.genesisConfig().networkParameters().chainId(),
            recoveryState.height(),
            recoveryState.round()
        );
    if (expectedProposer.empty() ||
        recoveryState.proposerAddress() != expectedProposer) {
        return false;
    }
    m_consensusLoop->loadFromRecoveryState(recoveryState);

    // Wire the block producer callback.
    // Returns a validated candidate block (no QC, no finalization).
    // ConsensusEventLoop handles adding the block to the chain, broadcasting
    // the proposal, voting, and finalization.
    m_consensusLoop->setBlockProducerCallback(
        [this](std::uint64_t height,
               std::uint64_t round,
               std::int64_t  now) -> std::optional<core::Block> {
            return this->produceBlock(height, round, now);
        }
    );

    // Wire the evidence pool for double-vote detection.
    m_consensusLoop->setEvidencePool(&m_evidencePool);

    // Wire a finalized-block callback that broadcasts the block to peers.
    // The authoritative
    // block artifact and runtime manifest are already committed atomically by
    // RuntimeBlockPipeline before this callback runs.
    m_consensusLoop->setFinalizedCallback(
        [this](const consensus::FinalizedBlockRecord& rec) {
            if (!m_tcpRuntime || m_runtime->blockchain().empty()) return;

            const auto& blocks = m_runtime->blockchain().blocks();
            if (rec.blockIndex() >= blocks.size()) return;

            const core::Block& block = blocks[static_cast<std::size_t>(rec.blockIndex())];
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            try {
                for (const consensus::DoubleVoteEvidence& evidence :
                     CanonicalSlashingTransition::evidenceFromBlock(block)) {
                    m_evidencePool.removeEvidence(evidence.evidenceId());
                }
            } catch (const std::exception& e) {
                m_syncHealth.recordBatchFailure(
                    std::string("Evidence pool cleanup error after finalization: ") + e.what(),
                    static_cast<std::int64_t>(now)
                );
                return;
            } catch (...) {
                m_syncHealth.recordBatchFailure(
                    "Evidence pool cleanup unknown error after finalization.",
                    static_cast<std::int64_t>(now)
                );
                return;
            }

            BlockAnnounceHandler::broadcastBlock(
                block,
                m_tcpRuntime->gossipMesh(),
                now
            );

            // Every 100 finalized blocks save an account-state snapshot so the
            // next restart can do partial replay instead of O(N) full replay.
            constexpr std::uint64_t kSnapshotInterval = 100;
            if (block.index() > 0 && block.index() % kSnapshotInterval == 0) {
                try {
                    const auto& genesisConfig =
                        m_runtime->config().genesisConfig();
                    const std::uint64_t minFeeRaw = genesisConfig
                        .networkParameters().minimumFeeRawUnits();
                    const std::int64_t minFee =
                        (minFeeRaw > static_cast<std::uint64_t>(
                                         std::numeric_limits<std::int64_t>::max()))
                        ? std::numeric_limits<std::int64_t>::max()
                        : static_cast<std::int64_t>(minFeeRaw);

                    const core::AccountStateView view =
                        RuntimeAccountStateBuilder::accountStateViewAtTip(
                            genesisConfig, m_runtime->blockchain(), minFee
                        );

                    storage::AccountStateSnapshotStore snapshotStore(
                        m_config.dataDirectory().rootPath()
                    );
                    snapshotStore.save(storage::AccountStateSnapshot(
                        genesisConfig.deterministicId(),
                        block.index(),
                        block.hash(),
                        view
                    ));
                } catch (...) {
                    // Snapshot failure is non-fatal; full replay will handle
                    // the next restart.
                }
            }
        }
    );

    m_consensusLoop->start(m_config.consensusTickMs());
    return true;
}

bool NodeOrchestrator::startRpc() {
    m_rpcStartError.clear();
    try {
        m_rpcServer = std::make_unique<NodeRpcServer>(
            *m_runtime,
            m_tcpRuntime->gossipMesh(),
            m_config.rpcPort(),
            m_config.rpcBindAddr()
        );
        m_rpcServer->start();
        return true;
    } catch (const std::exception& error) {
        // RPC failure is non-fatal — node still participates in consensus.
        m_rpcStartError = error.what();
        m_rpcServer.reset();
        return false;
    } catch (...) {
        m_rpcStartError = "Unknown RPC startup error.";
        m_rpcServer.reset();
        return false;
    }
}

void NodeOrchestrator::setLocalSigner(crypto::Signer signer) {
    m_localSigner = std::move(signer);
}

void NodeOrchestrator::setLocalNodeIdentity(
    crypto::KeyPair nodeIdentityKey
) {
    if (!nodeIdentityKey.isValid() ||
        nodeIdentityKey.algorithm() !=
            crypto::CryptoAlgorithm::CLASSIC_ED25519) {
        throw std::invalid_argument(
            "Local node identity requires a valid Ed25519 key pair."
        );
    }
    m_localNodeIdentity = std::move(nodeIdentityKey);
}

std::optional<core::Block> NodeOrchestrator::produceBlock(
    std::uint64_t height,
    std::uint64_t round,
    std::int64_t  now
) {
    if (m_runtime->validatorRegistry().activeCount() == 0) return std::nullopt;

    // Confirm this node is the designated proposer for (height, round).
    const std::string chainId =
        m_config.genesisConfig().networkParameters().chainId();
    const std::string expectedProposer = consensus::ProposerSchedule::selectProposer(
        m_runtime->validatorRegistry(), chainId, height, round
    );

    if (expectedProposer != m_config.localValidatorAddress()) return std::nullopt;
    if (!m_localSigner.has_value()) return std::nullopt;

    // Production only — no voting, no QC, no finalization.
    // ConsensusEventLoop is responsible for adding the block to the chain,
    // broadcasting the proposal, voting, and finalizing.
    const std::vector<consensus::DoubleVoteEvidence> pendingEvidence =
        m_evidencePool.doubleVoteEvidenceBeforeHeight(height);
    const std::size_t minimumTransactions =
        pendingEvidence.empty() ? 1U : 0U;
    const RuntimeBlockPipelineConfig pipelineConfig(
        m_config.maxBlockTransactions(), minimumTransactions, round, now
    );

    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(
            *m_runtime,
            pipelineConfig,
            pendingEvidence
        );

    if (!candidate.produced()) return std::nullopt;

    return candidate.block();
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

std::optional<p2p::PeerMetadata> NodeOrchestrator::localHandshakePeer(
    std::int64_t now
) const {
    if (!m_tcpRuntime || !m_localNodeIdentity.has_value() || now <= 0) {
        return std::nullopt;
    }
    return p2p::PeerMetadata(
        m_config.localPeer().peerId(),
        m_tcpRuntime->transport().localEndpoint(),
        m_localNodeIdentity->publicKey().fingerprint(),
        now,
        now,
        0,
        false
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
            unsigned long parsedPort = std::stoul(endpoint.substr(colon + 1));
            if (parsedPort > 0 && parsedPort <= 65535) {
                port = static_cast<std::uint16_t>(parsedPort);
            } else {
                throw std::invalid_argument("Port out of range in endpoint: " + endpoint);
            }
        } catch (...) {
            throw std::invalid_argument("Invalid port for peer endpoint: " + endpoint);
        }
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
