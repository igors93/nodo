#include "node/NodeDaemon.hpp"

#include "node/FinalizedArtifactGossipAdmission.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "p2p/Peer.hpp"

#include <chrono>
#include <cstdlib>
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

void NodeDaemon::setLocalNodeIdentity(
    crypto::KeyPair nodeIdentityKey
) {
    m_orchestrator.setLocalNodeIdentity(std::move(nodeIdentityKey));
}

void NodeDaemon::tick(std::int64_t now) {
    m_orchestrator.tick(now);
    processTransactionGossip(now);
    processFinalizedArtifacts();
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

        m_orchestrator.registerBootstrapPeer(meta, now);
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

        const auto decoded = PersistentMempoolStore::deserializeGossip(
            payload,
            m_policy,
            crypto::SecurityContext::USER_TRANSACTION,
            m_orchestrator.runtime().config().genesisConfig()
                .networkParameters().chainId()
        );

        bool admitted = false;
        if (decoded.has_value()) {
            NodeRuntime& runtime = m_orchestrator.mutableRuntime();
            const auto& network = runtime.config().genesisConfig().networkParameters();
            const crypto::ProtocolCryptoContext cryptoContext =
                crypto::ProtocolCryptoContext::fromNetworkName(network.networkName());
            if (cryptoContext.isValid()) {
                const core::AccountStateView accounts = runtime.cachedAccountStateAtTip(
                    static_cast<std::int64_t>(runtime.effectiveMinimumFeeRawUnits())
                );
                const TransactionAdmissionContext admissionContext(
                    accounts, runtime.mempool(), runtime.stakingRegistry(),
                    runtime.validatorRegistry(), runtime.governanceExecutor(),
                    runtime.blockchain().size()
                );
                const TransactionAdmissionResult validation =
                    TransactionAdmissionValidator::validateNetworkSubmission(
                        decoded->transaction, network, accounts, runtime.mempool(),
                        cryptoContext.policy(), crypto::SecurityContext::USER_TRANSACTION,
                        cryptoContext.userSignatureProvider(),
                        runtime.effectiveMinimumFeeRawUnits(), &admissionContext
                    );
                if (validation.accepted()) {
                    admitted = runtime.mutableMempool().admitTransaction(
                        decoded->transaction, m_policy,
                        crypto::SecurityContext::USER_TRANSACTION,
                        decoded->acceptedAt
                    ).success();
                }
            }
        }

        if (admitted) {
            m_orchestrator.gossipBroadcast(
                p2p::NetworkMessageType::TRANSACTION_GOSSIP,
                payload,
                now
            );
        }
    }
}

void NodeDaemon::processFinalizedArtifacts() {
    const auto messages = m_orchestrator.drainGossipInbox(
        p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT
    );

    FinalizedBlockRecordStore store(
        m_config.orchestratorConfig.dataDirectory().rootPath()
    );

    for (const auto& envelope : messages) {
        const FinalizedArtifactGossipAdmissionResult result =
            FinalizedArtifactGossipAdmission::admit(
                envelope,
                m_orchestrator.mutableRuntime(),
                m_policy,
                m_provider,
                store
            );
        if (result.fatalConsistencyError()) {
            // Finality must never remain memory-only or diverge from its
            // durable proof. Preserve the existing fail-stop safety policy.
            std::abort();
        }
    }
}

} // namespace nodo::node
