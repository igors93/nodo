#include "consensus/ConsensusEventLoop.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <chrono>
#include <thread>

namespace nodo::consensus {

ConsensusEventLoop::ConsensusEventLoop(
    node::NodeRuntime&               runtime,
    p2p::GossipMesh&                 gossip,
    const crypto::CryptoPolicy&      policy,
    const crypto::SignatureProvider&  provider
)
    : m_runtime(runtime)
    , m_gossip(gossip)
    , m_policy(policy)
    , m_provider(provider)
    , m_running(false)
    , m_tickIntervalMs(DEFAULT_TICK_INTERVAL_MS)
{}

ConsensusEventLoop::~ConsensusEventLoop() {
    stop();
}

void ConsensusEventLoop::setFinalizedCallback(FinalizedCallback cb) {
    m_onFinalized = std::move(cb);
}

void ConsensusEventLoop::start(std::int64_t tickIntervalMs) {
    if (m_running.load()) return;
    m_tickIntervalMs = tickIntervalMs;
    m_running.store(true);
    m_thread = std::thread([this]{ runLoop(); });
}

void ConsensusEventLoop::stop() {
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool ConsensusEventLoop::isRunning() const {
    return m_running.load();
}

void ConsensusEventLoop::runLoop() {
    while (m_running.load()) {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        tick(static_cast<std::int64_t>(now));

        std::this_thread::sleep_for(
            std::chrono::milliseconds(m_tickIntervalMs)
        );
    }
}

ConsensusTickResult ConsensusEventLoop::tick(std::int64_t now) {
    ConsensusTickResult result = drainVotesAndCollect(now);

    // If votes were collected, try to form a quorum on the current tip.
    const core::Blockchain& chain = m_runtime.blockchain();
    if (chain.empty()) return result;

    const core::Block& tip = chain.latestBlock();
    const std::uint64_t height = tip.index();
    const std::string& blockHash = tip.hash();
    const std::string& prevHash  = tip.previousHash();

    const auto& state = m_runtime.consensusRoundManager().currentState();
    const std::uint64_t round = state.round();

    if (tryFinalizeBlock(height, blockHash, prevHash, round, now, result)) {
        if (m_onFinalized && result.blockFinalized) {
            const FinalizedBlockRecord* rec =
                m_finalizationRegistry.recordForHeight(height);
            if (rec) m_onFinalized(*rec);
        }
    }

    // Check round timeout and advance if expired.
    if (m_runtime.advanceConsensusRoundIfTimedOut(now)) {
        result.roundAdvanced = true;
    }

    return result;
}

ConsensusTickResult ConsensusEventLoop::drainVotesAndCollect(
    std::int64_t /*now*/
) {
    ConsensusTickResult result;

    const auto& inbox = m_gossip.inbox();
    const auto voteMessages = inbox.messagesForType(
        p2p::NetworkMessageType::VOTE_ANNOUNCE
    );

    for (const auto& envelope : voteMessages) {
        if (envelope.payload().empty()) continue;

        ValidatorVoteRecord vote =
            ValidatorVoteRecord::deserialize(envelope.payload());

        if (!vote.isStructurallyValid(m_policy)) continue;

        const VoteCollectResult collected =
            m_runtime.submitConsensusVote(vote);

        if (collected.accepted()) {
            result.votesCollected++;
        }
    }

    return result;
}

bool ConsensusEventLoop::tryFinalizeBlock(
    std::uint64_t       blockIndex,
    const std::string&  blockHash,
    const std::string&  previousHash,
    std::uint64_t       round,
    std::int64_t        now,
    ConsensusTickResult& result
) {
    if (m_finalizationRegistry.hasFinalizedHeight(blockIndex)) {
        return false;
    }

    const NetworkVoteCollector& collector =
        m_runtime.consensusRoundManager().voteCollector();

    const VotePool& pool = collector.votePool();

    const QuorumCertificateBuildResult qcResult =
        QuorumAssembly::tryBuildCertificate(
            pool,
            blockIndex,
            blockHash,
            previousHash,
            round,
            m_runtime.validatorRegistry(),
            m_policy,
            m_provider,
            QUORUM_NUMERATOR,
            QUORUM_DENOMINATOR
        );

    if (!qcResult.certified()) return false;

    result.quorumFormed = true;

    const core::Block& block = m_runtime.blockchain().latestBlock();

    BlockFinalizationResult finResult = BlockFinalizer::finalizeBlock(
        m_runtime.mutableBlockchain(),
        block,
        qcResult.certificate(),
        m_runtime.validatorRegistry(),
        m_finalizationRegistry,
        m_policy,
        m_provider,
        now
    );

    if (!finResult.success()) {
        if (!finResult.duplicate()) {
            result.errorMessage = finResult.reason();
        }
        return false;
    }

    result.blockFinalized     = true;
    result.finalizedBlockHash = finResult.record().blockHash();
    result.finalizedHeight    = finResult.record().blockIndex();

    return true;
}

} // namespace nodo::consensus
