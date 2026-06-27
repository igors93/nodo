#include "node/NodeDaemon.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "p2p/Peer.hpp"

#include <chrono>
#include <thread>

namespace nodo::node {

NodeDaemon::NodeDaemon(
    NodeDaemonConfig                 config,
    const crypto::CryptoPolicy&      policy,
    const crypto::SignatureProvider&  provider
)
    : m_config(std::move(config))
    , m_policy(policy)
    , m_provider(provider)
    , m_orchestrator(m_config.orchestratorConfig, policy, provider)
    , m_seenTxCache(m_config.seenCacheMaxEntries, m_config.seenCacheTtlSeconds)
    , m_running(false)
{}

NodeDaemon::~NodeDaemon() {
    stop();
}

NodeOrchestratorStartResult NodeDaemon::start() {
    auto result = m_orchestrator.start();
    if (!result.running()) return result;

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    registerStaticPeers(now);
    m_running.store(true);
    return result;
}

void NodeDaemon::runBlocking(std::int64_t tickIntervalMs) {
    while (m_running.load()) {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        tick(static_cast<std::int64_t>(now));

        std::this_thread::sleep_for(std::chrono::milliseconds(tickIntervalMs));
    }
}

void NodeDaemon::stop() {
    m_running.store(false);
    m_orchestrator.stop();
}

bool NodeDaemon::isRunning() const {
    return m_running.load();
}

void NodeDaemon::setLocalSigner(crypto::Signer signer) {
    m_orchestrator.setLocalSigner(std::move(signer));
}

void NodeDaemon::tick(std::int64_t now) {
    m_orchestrator.tick(now);
    processTransactionGossip(now);
    processFinalizedArtifacts(now);
}

const NodeOrchestrator& NodeDaemon::orchestrator() const {
    return m_orchestrator;
}

void NodeDaemon::registerStaticPeers(std::int64_t now) {
    for (const auto& peer : m_config.staticPeers) {
        if (!peer.isValid()) continue;

        const p2p::PeerMetadata meta(
            peer.nodeId,
            p2p::PeerEndpoint(peer.host, peer.port),
            "",
            now,
            now,
            0,
            false
        );

        m_orchestrator.addAndConnectPeer(meta);
    }
}

void NodeDaemon::processTransactionGossip(std::int64_t now) {
    const auto messages = m_orchestrator.drainGossipInbox(
        p2p::NetworkMessageType::TRANSACTION_GOSSIP
    );

    for (const auto& envelope : messages) {
        const std::string& payload = envelope.payload();
        if (payload.empty()) continue;

        // Use payload hash as dedup key: two peers forwarding the same tx
        // have the same payloadHash but different envelope messageIds.
        const std::string& cacheKey = envelope.payloadHash();
        if (cacheKey.empty()) continue;

        m_seenTxCache.evictExpired(now);

        if (!m_seenTxCache.markSeen(cacheKey, now)) {
            continue; // already processed this tx this window
        }

        const bool admitted = PersistentMempoolStore::deserializeGossipAndAdmit(
            payload,
            m_orchestrator.mutableRuntime().mutableMempool(),
            m_policy,
            crypto::SecurityContext::USER_TRANSACTION
        );

        if (admitted) {
            m_orchestrator.gossipBroadcast(
                p2p::NetworkMessageType::TRANSACTION_GOSSIP,
                payload,
                now
            );
        }
    }
}

void NodeDaemon::processFinalizedArtifacts(std::int64_t now) {
    const auto messages = m_orchestrator.drainGossipInbox(
        p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT
    );

    for (const auto& envelope : messages) {
        if (envelope.payload().empty()) continue;

        try {
            const consensus::FinalizedBlockRecord record =
                consensus::FinalizedBlockRecord::deserialize(envelope.payload());

            if (!record.isStructurallyValid()) continue;

            // Verify the QC against the local validator registry.
            if (!record.verify(
                    m_orchestrator.runtime().validatorRegistry(),
                    m_policy,
                    m_provider)) {
                continue;
            }

            // Record in the local finalization registry.
            const auto regResult = m_orchestrator.mutableRuntime()
                .mutableFinalizationRegistry()
                .registerFinalizedBlock(record);

            // Persist to disk so BlockSyncQcMode::QC_REQUIRED works after
            // restart and so sync responses to peers carry QC proofs.
            if (regResult.registered()) {
                try {
                    FinalizedBlockRecordStore qcStore(
                        m_config.orchestratorConfig.dataDirectory().rootPath()
                    );
                    qcStore.save(record);
                } catch (...) {}
            }
        } catch (...) {
            // Malformed artifact from peer — silently discard.
        }
    }

    (void)now;
}

} // namespace nodo::node
