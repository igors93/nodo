#include "node/NodeDaemon.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "node/BlockAnnounceHandler.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "p2p/Peer.hpp"
#include "serialization/BlockCodec.hpp"

#include <chrono>
#include <limits>
#include <optional>
#include <thread>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnitsForRuntime(
    const NodeRuntime& runtime
) {
    const std::uint64_t minimumFee =
        runtime.config().genesisConfig().networkParameters().minimumFeeRawUnits();

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(minimumFee);
}

core::StateTransitionPreviewContext previewContextForRuntime(
    const NodeRuntime& runtime
) {
    return RuntimeAccountStateBuilder::previewContextAtTip(
        runtime.config().genesisConfig(),
        runtime.blockchain(),
        minimumFeeRawUnitsForRuntime(runtime)
    );
}

} // namespace

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
    processBlockProposals(now);
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

void NodeDaemon::processBlockProposals(std::int64_t now) {
    const auto messages = m_orchestrator.drainGossipInbox(
        p2p::NetworkMessageType::BLOCK_PROPOSAL
    );

    const core::Blockchain& chain = m_orchestrator.runtime().blockchain();
    const core::ValidatorRegistry& validatorRegistry =
        m_orchestrator.runtime().validatorRegistry();

    for (const auto& envelope : messages) {
        if (envelope.payload().empty()) continue;

        // Decode as a SignedBlockProposalMessage. Proposals that cannot be
        // decoded (e.g. raw Block bytes from older peers) are dropped.
        SignedBlockProposalMessage proposal;
        bool decodeFailed = false;
        try {
            proposal = SignedBlockProposalMessage::deserialize(envelope.payload());
        } catch (...) {
            decodeFailed = true;
        }
        if (decodeFailed || !proposal.isValid()) continue;

        // Verify the proposer identity against the schedule and signature.
        const std::uint64_t activeValidators = validatorRegistry.activeCount();
        if (activeValidators > 0) {
            const std::string chainId =
                m_orchestrator.config().genesisConfig().networkParameters().chainId();
            const std::uint64_t currentRound =
                m_orchestrator.runtime().consensusRoundManager().currentState().round();
            const std::string expectedProposer =
                consensus::ProposerSchedule::selectProposer(
                    validatorRegistry,
                    chainId,
                    proposal.blockIndex(),
                    currentRound
                );

            if (!proposal.verify(
                    expectedProposer,
                    validatorRegistry,
                    m_orchestrator.cryptoPolicy(),
                    m_orchestrator.signatureProvider()
                )) {
                continue;
            }
        }

        // Deserialize the block from the verified proposal payload.
        std::optional<core::Block> blockOpt;
        try {
            blockOpt = serialization::BlockCodec::deserialize(proposal.serializedBlock());
        } catch (...) {
            continue;
        }

        if (!blockOpt || !blockOpt->isValid()) continue;

        const core::Block& block = *blockOpt;

        // The signed proposal commits to the block hash; verify it matches.
        if (block.hash() != proposal.blockHash() ||
            block.index() != proposal.blockIndex()) {
            continue;
        }

        if (!chain.canAppendBlock(block)) continue;

        core::StateTransitionPreviewContext validationContext;
        try {
            validationContext = previewContextForRuntime(m_orchestrator.runtime());
        } catch (const std::exception&) {
            continue;
        }

        const core::BlockValidationResult validation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                chain,
                block,
                validationContext
            );
        if (!validation.accepted()) continue;

        m_orchestrator.mutableRuntime().mutableBlockchain().addBlock(block);
    }

    (void)now;
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
            m_orchestrator.mutableRuntime()
                .mutableFinalizationRegistry()
                .registerFinalizedBlock(record);
        } catch (...) {
            // Malformed artifact from peer — silently discard.
        }
    }

    (void)now;
}

} // namespace nodo::node
