#include "node/NodeOrchestrator.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "crypto/Hex.hpp"
#include "crypto/Signer.hpp"
#include "node/BlockAnnounceHandler.hpp"
#include "node/BlockSyncHandler.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/PeerHandshakeAutoRegistrar.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "serialization/ProtocolMessageCodec.hpp"
#include "storage/AccountStateSnapshotStore.hpp"
#include "storage/BlockFileStore.hpp"

#include <chrono>
#include <filesystem>
#include <limits>
#include <map>
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
// Internal persistence helper (file-scope)
// ---------------------------------------------------------------------------

namespace {

static const std::string kOrchestratorCanonicalPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

// Persists a finalized block to disk and advances the durable sync checkpoint.
// Called from both the FinalizedCallback (consensus-driven finalization) and
// after block sync applies new blocks. Errors are swallowed: storage failures
// must not crash the node.
void persistFinalizedBlock(
    const core::Block& block,
    const std::filesystem::path& dataRoot,
    const std::string& sourcePeerId,
    std::int64_t now
) {
    try {
        storage::BlockFileStore store(dataRoot.string());
        store.writeBlock(block);
    } catch (...) {}

    try {
        PersistentSyncCheckpointStore checkpointStore(dataRoot);
        const std::string stateRoot = block.hasCanonicalStateRoot()
            ? block.stateRoot()
            : "height-" + std::to_string(block.index());

        checkpointStore.save(PersistentSyncCheckpoint(
            PersistentSyncCheckpoint::SCHEMA_VERSION,
            block.index(),
            block.hash(),
            stateRoot,
            PersistentSyncStatus::COMPLETE,
            sourcePeerId,
            now
        ));
    } catch (...) {}
}

// Persists a FinalizedBlockRecord (QC proof) for a finalized block to the
// durable QC store. Called whenever a block is finalized — either by local
// consensus or via gossip. Errors are swallowed: storage failures must not
// crash the node.
void persistFinalizedRecord(
    const consensus::FinalizedBlockRecord& record,
    const std::filesystem::path& dataRoot
) {
    try {
        FinalizedBlockRecordStore qcStore(dataRoot);
        qcStore.save(record);
    } catch (...) {}
}

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
    const FinalizedBlockRecordStore& qcStore,
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

        const auto qcOpt = qcStore.load(block.index());
        const std::string serializedQc =
            qcOpt.has_value() ? qcOpt->serialize() : "";

        items.emplace_back(
            block.index(),
            block.hash(),
            block.previousHash(),
            block.serialize(),
            block.hasCanonicalStateRoot()
                ? block.stateRoot()
                : "height-" + std::to_string(block.index()),
            now,
            serializedQc
        );
    }
    if (items.empty()) return PersistentBlockSyncBatch{};
    return PersistentBlockSyncBatch(
        localPeerId,
        items.front().height(),
        items.back().height(),
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

    // Snapshot our chain height once. Used throughout this tick.
    const std::uint64_t localHeight =
        m_runtime->blockchain().empty() ? 0
        : m_runtime->blockchain().latestBlock().index();

    // Compute effective minimum fee once. effectiveMinimumFeeRawUnits() checks any
    // applied governance overrides before falling back to the genesis config value.
    const config::GenesisConfig& genesisConfig = m_runtime->config().genesisConfig();
    const std::uint64_t minFeeRaw = m_runtime->effectiveMinimumFeeRawUnits();
    const std::int64_t minFee =
        (minFeeRaw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        ? std::numeric_limits<std::int64_t>::max()
        : static_cast<std::int64_t>(minFeeRaw);

    // 2. Auto-register new peers that sent PEER_HELLO.
    PeerHandshakeAutoRegistrar::processInbox(
        gossip,
        currentChainStatus(),
        now
    );

    // 2.5. Drain incoming CHAIN_STATUS messages.
    // These are broadcast by peers after round advances or on handshake. We use
    // them to keep our local peer-height registry accurate and to trigger
    // persistent sync when a peer is materially ahead of us.
    {
        const auto chainStatusMsgs = gossip.drainInbox(
            p2p::NetworkMessageType::CHAIN_STATUS
        );
        for (const auto& envelope : chainStatusMsgs) {
            const auto optBytes = tryUnwrapCanonical(envelope.payload());
            if (!optBytes.has_value()) continue;
            try {
                const ChainStatusMessage status =
                    serialization::ProtocolMessageCodec::decodeChainStatusMessage(
                        optBytes.value()
                    );
                if (!status.isValid()) continue;

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
                    triggerSyncIfBehind(status, now);
                }
            } catch (...) {}
        }
    }

    // Build a protocol validation context from the current chain state.
    // Used by both BLOCK_ANNOUNCE and BLOCK_RESPONSE validation below.
    // wallClockNow is passed so the validator can reject blocks with timestamps
    // too far in the future. Falls back to structural-only on context failure.
    // The account state view is served from the NodeRuntime cache: it is only
    // rebuilt when the chain tip height has changed since the last tick.
    core::StateTransitionPreviewContext announceValidationContext;
    bool announceContextValid = true;
    try {
        announceValidationContext = core::StateTransitionPreviewContext(
            minFee,
            m_runtime->cachedAccountStateAtTip(minFee),
            false,
            true,
            "",
            now
        );
    } catch (...) {
        announceContextValid = false;
    }

    // 3. Apply any incoming BLOCK_ANNOUNCE messages.
    if (announceContextValid) {
        BlockAnnounceHandler::processInbox(
            gossip,
            m_runtime->mutableBlockchain(),
            announceValidationContext,
            now
        );
    } else {
        gossip.drainInbox(p2p::NetworkMessageType::BLOCK_ANNOUNCE);
    }

    // 4. Serve incoming BLOCK_REQUEST messages (fast-path sync).
    BlockSyncHandler::serveRequests(
        gossip,
        m_runtime->blockchain(),
        now
    );

    // 4.5. Serve incoming BLOCK_SYNC_REQUEST messages (persistent sync path).
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

                const std::uint64_t maxItems = std::min(
                    req.locator().maxBlocks(),
                    static_cast<std::uint64_t>(BlockSyncHandler::MAX_BLOCKS_PER_RESPONSE)
                );
                const FinalizedBlockRecordStore qcStoreForResponse(
                    m_config.dataDirectory().rootPath()
                );
                const PersistentBlockSyncBatch batch = buildSyncResponseBatch(
                    m_runtime->blockchain(),
                    qcStoreForResponse,
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
            } catch (...) {}
        }
    }

    // 5. Apply any received BLOCK_RESPONSE messages (fast-path sync).
    // The context builder is called per-block so the state reflects all blocks
    // applied so far in the batch. wallClockNow is captured so future-timestamp
    // drift is enforced even during sync. Each callback invalidates the cache so
    // the rebuilt view always matches the chain tip at the time of the call.
    std::size_t syncApplied = 0;
    try {
        syncApplied = BlockSyncHandler::applyResponses(
            gossip,
            m_runtime->mutableBlockchain(),
            [this, minFee, now](const core::Blockchain&) -> core::StateTransitionPreviewContext {
                try {
                    m_runtime->invalidateAccountStateCache();
                    return core::StateTransitionPreviewContext(
                        minFee,
                        m_runtime->cachedAccountStateAtTip(minFee),
                        false,
                        true,
                        "",
                        now
                    );
                } catch (...) {
                    throw std::runtime_error("State load failed, aborting block sync.");
                }
            },
            m_runtime->finalizationRegistry(),
            BlockSyncQcMode::QC_REQUIRED,
            now
        );
    } catch (...) {
        // Fail gracefully instead of bypassing consensus validation
    }

    // Persist each block that was successfully synced via fast-path.
    // Also persist the QC record from the finalization registry — the
    // QC_REQUIRED mode already verified these records before applying.
    if (syncApplied > 0 && !m_runtime->blockchain().empty()) {
        const FinalizedBlockRecordStore qcStore(m_config.dataDirectory().rootPath());
        const auto& allBlocks = m_runtime->blockchain().blocks();
        const std::uint64_t newHeight = m_runtime->blockchain().latestBlock().index();
        const std::uint64_t firstSynced =
            (newHeight >= static_cast<std::uint64_t>(syncApplied))
            ? newHeight - static_cast<std::uint64_t>(syncApplied) + 1
            : 0;
        for (const auto& b : allBlocks) {
            if (b.index() >= firstSynced && b.index() <= newHeight) {
                m_runtime->applyGovernanceFromBlock(b, now);
                persistFinalizedBlock(
                    b,
                    m_config.dataDirectory().rootPath(),
                    "sync",
                    now
                );
                const auto* rec =
                    m_runtime->finalizationRegistry().recordForHeight(b.index());
                if (rec != nullptr) {
                    persistFinalizedRecord(*rec, m_config.dataDirectory().rootPath());
                }
            }
        }
    }

    // 5.5. Apply any received BLOCK_SYNC_RESPONSE batches (persistent sync path).
    // Each batch is validated block-by-block with ProtocolCommitment mode before
    // being added to the chain. Applied blocks are persisted immediately.
    {
        const auto syncResponses = gossip.drainInbox(
            p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE
        );

        PersistentSyncCheckpointStore cpStore(m_config.dataDirectory().rootPath());
        const PersistentSyncCheckpointReadResult readCp = cpStore.read();
        PersistentSyncCheckpoint currentCheckpoint = readCp.loaded()
            ? readCp.checkpoint()
            : PersistentSyncCheckpoint::genesis(
                m_runtime->blockchain().empty()
                    ? "genesis"
                    : m_runtime->blockchain().latestBlock().hash(),
                "genesis-state-root",
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

                const PersistentSyncApplyResult applyResult =
                    PersistentBlockStateSyncApplier::applyValidatedBatch(
                        currentCheckpoint,
                        batch,
                        m_runtime->mutableBlockchain(),
                        m_runtime->validatorRegistry(),
                        m_policy,
                        m_provider,
                        [this, minFee, now](
                            const core::Blockchain&
                        ) -> core::StateTransitionPreviewContext {
                            try {
                                m_runtime->invalidateAccountStateCache();
                                return core::StateTransitionPreviewContext(
                                    minFee,
                                    m_runtime->cachedAccountStateAtTip(minFee),
                                    false,
                                    true,
                                    "",
                                    now
                                );
                            } catch (...) {
                                throw std::runtime_error("State load failed, aborting block sync batch.");
                            }
                        },
                        now
                    );

                if (applyResult.applied()) {
                    // Advance the in-memory checkpoint so the next batch in this
                    // same tick starts from the correct height, then persist it
                    // so restarts never re-request blocks we already have.
                    currentCheckpoint = applyResult.checkpoint().value();
                    cpStore.save(currentCheckpoint);

                    // Build a height→serializedQc lookup from batch items that
                    // carry a FinalizedBlockRecord so we can persist the QC
                    // alongside each block without searching the batch twice.
                    std::map<std::uint64_t, std::string> serializedQcByHeight;
                    for (const auto& item : batch.items()) {
                        if (!item.serializedFinalizedRecord().empty()) {
                            serializedQcByHeight[item.height()] =
                                item.serializedFinalizedRecord();
                        }
                    }

                    const FinalizedBlockRecordStore qcStore(
                        m_config.dataDirectory().rootPath()
                    );
                    const auto& allBlocks = m_runtime->blockchain().blocks();
                    for (const auto& b : allBlocks) {
                        if (b.index() >= batch.fromHeight() &&
                            b.index() <= batch.toHeight()) {
                            m_runtime->applyGovernanceFromBlock(b, now);
                            persistFinalizedBlock(
                                b,
                                m_config.dataDirectory().rootPath(),
                                batch.sourcePeerId(),
                                now
                            );
                            const auto it = serializedQcByHeight.find(b.index());
                            if (it != serializedQcByHeight.end()) {
                                try {
                                    const auto rec =
                                        consensus::FinalizedBlockRecord::deserialize(
                                            it->second
                                        );
                                    persistFinalizedRecord(
                                        rec,
                                        m_config.dataDirectory().rootPath()
                                    );
                                } catch (...) {}
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // 5.8. Process any new slashing evidence accumulated by the ConsensusEventLoop.
    // For each piece of evidence not already in the penalty ledger, compute a
    // deterministic penalty decision and propagate its registry effect immediately
    // so jailed/tombstoned validators lose consensus eligibility this tick.
    {
        const std::uint64_t chainHeight = m_runtime->blockchain().empty()
            ? 0 : m_runtime->blockchain().latestBlock().index();
        const std::uint64_t epochDuration =
            genesisConfig.networkParameters().epochDurationSeconds();
        const std::uint64_t currentEpoch =
            (epochDuration > 0) ? (chainHeight / epochDuration) : 0;

        const consensus::ValidatorPenaltyPolicy penaltyPolicy =
            consensus::ValidatorPenaltyPolicy::conservativeTestnetPolicy();

        for (const auto& evidence : m_evidencePool.allEvidence()) {
            if (!m_penaltyLedger.containsEvidence(evidence.evidenceId())) {
                m_penaltyLedger.applyEvidenceWithRegistryEffect(
                    evidence,
                    penaltyPolicy,
                    now,
                    currentEpoch,
                    m_runtime->mutableValidatorRegistry()
                );
            }
        }
    }

    // 6. If a known peer is ahead of us, request the next missing blocks.
    // Only trust peers that have completed handshake (non-empty peerId and endpoint).
    // Cap the look-ahead window to guard against height-spoofing attacks.
    static constexpr std::uint64_t kMaxSyncLookAhead = 10000;
    static std::int64_t lastFastSyncMs = 0;
    const std::uint64_t currentHeight =
        m_runtime->blockchain().empty() ? 0
        : m_runtime->blockchain().latestBlock().index();
    const auto peers = m_runtime->peerManager().peers();
    
    if (now - lastFastSyncMs >= 5) {
        for (const auto& peer : peers) {
            if (peer.peerId().empty() || peer.endpoint().empty()) continue;
            if (peer.latestKnownHeight() > currentHeight + kMaxSyncLookAhead) continue;
            if (peer.latestKnownHeight() > currentHeight + 1) {
                BlockSyncHandler::requestBlocks(
                    gossip,
                    m_config.localPeer().peerId(),
                    currentHeight + 1,
                    BlockSyncHandler::MAX_BLOCKS_PER_RESPONSE,
                    now
                );
                lastFastSyncMs = now;
                break;
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
    const PersistentSyncCheckpointReadResult readResult = store.read();
    if (readResult.loaded()) {
        checkpoint = readResult.checkpoint();
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
    // Use canonical hex encoding so the receiving node can decode with
    // ProtocolMessageCodec::decodeNetworkBlockSyncRequest (step 4.5 in tick()).
    const std::string payload = kOrchestratorCanonicalPrefix +
        crypto::hexEncode(
            serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(req)
        );
    m_tcpRuntime->gossipMesh().broadcast(
        p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
        payload,
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

    // Restore durable QC records into the finalization registry so
    // BlockSyncQcMode::QC_REQUIRED works correctly after restart and peers
    // requesting sync via BLOCK_SYNC_REQUEST receive batches with QC proofs.
    {
        FinalizedBlockRecordStore qcStore(
            m_config.dataDirectory().rootPath()
        );

        const auto& blocks = m_runtime->blockchain().blocks();

        for (const auto& record : qcStore.loadAll()) {
            if (record.blockIndex() >= blocks.size()) {
                continue;
            }

            const core::Block& block =
                blocks[static_cast<std::size_t>(record.blockIndex())];

            if (!record.matchesBlock(block)) {
                continue;
            }

            if (!record.verify(
                    m_runtime->validatorRegistry(),
                    m_policy,
                    m_provider
                )) {
                continue;
            }

            m_runtime->mutableFinalizationRegistry()
                .registerFinalizedBlock(record);
        }
    }

    // Replay governance transactions from all loaded blocks so effective
    // network parameters reflect any changes approved before this restart.
    {
        const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        for (const auto& block : m_runtime->blockchain().blocks()) {
            m_runtime->applyGovernanceFromBlock(
                block, static_cast<std::int64_t>(nowSec)
            );
        }
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
    m_consensusLoop->setDataDirectoryConfig(&m_config.dataDirectory());

    // Restore lock/vote state from disk so we don't double-vote after restart.
    const auto recoveryState =
        consensus::ConsensusRecoveryStore::load(recoveryPath);
    if (recoveryState.has_value()) {
        m_consensusLoop->loadFromRecoveryState(*recoveryState);
    }

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

    // Wire a finalized-block callback that broadcasts the block to peers,
    // announces it and saves a derived QC index for sync. The authoritative
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

            BlockAnnounceHandler::broadcastBlock(
                block,
                m_tcpRuntime->gossipMesh(),
                now
            );

            persistFinalizedRecord(rec, m_config.dataDirectory().rootPath());

            // Every 100 finalized blocks save an account-state snapshot so the
            // next restart can do partial replay instead of O(N) full replay.
            constexpr std::uint64_t kSnapshotInterval = 100;
            if (block.index() > 0 && block.index() % kSnapshotInterval == 0) {
                try {
                    const auto& genesisConfig =
                        m_runtime->config().genesisConfig();
                    const std::uint64_t minFeeRaw =
                        m_runtime->effectiveMinimumFeeRawUnits();
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

    // Phase 1: production only — no voting, no QC, no finalization.
    // ConsensusEventLoop is responsible for adding the block to the chain,
    // broadcasting the proposal, voting, and finalizing.
    const RuntimeBlockPipelineConfig pipelineConfig(
        m_config.maxBlockTransactions(), 0, round, now
    );

    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(*m_runtime, pipelineConfig);

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
