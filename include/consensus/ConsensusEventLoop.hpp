#ifndef NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP
#define NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP

#include "consensus/BlockFinalizer.hpp"
#include "consensus/ConsensusRoundManager.hpp"
#include "consensus/EvidencePool.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "consensus/VotePool.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/GossipMesh.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>

namespace nodo::consensus {

/*
 * ConsensusTickResult reports what happened during one event-loop tick.
 *
 * Fields are intentionally additive: multiple things can happen in the same
 * tick (e.g., votes collected AND quorum formed AND block finalized).
 */
struct ConsensusTickResult {
    std::uint32_t votesCollected  = 0;
    bool          quorumFormed    = false;
    bool          blockFinalized  = false;
    bool          roundAdvanced   = false;
    std::string   finalizedBlockHash;
    std::uint64_t finalizedHeight = 0;
    std::string   errorMessage;

    bool hasError() const { return !errorMessage.empty(); }
};

/*
 * ConsensusEventLoop drives BFT round advancement, vote collection and
 * automatic block finalization from a background thread.
 *
 * Design:
 * - Stateless from the caller's perspective: all mutable state lives in
 *   NodeRuntime and GossipMesh.
 * - tick() can be called manually for tests (thread is optional).
 * - Uses QuorumAssembly to attempt certificate construction every tick.
 * - On quorum success, delegates to BlockFinalizer and advances height.
 *
 * Thread safety:
 * The background thread calls tick() in a tight loop with a sleep between
 * iterations. Callers must not call tick() concurrently with the thread.
 * To run single-threaded (e.g., tests), call stop() first.
 */
class ConsensusEventLoop {
public:
    static constexpr std::int64_t  DEFAULT_TICK_INTERVAL_MS  = 100;
    static constexpr std::uint64_t QUORUM_NUMERATOR          = 2;
    static constexpr std::uint64_t QUORUM_DENOMINATOR        = 3;

    /*
     * Callback invoked after a block is successfully finalized.
     * Can be used to notify application layers (e.g., mempool pruning,
     * state snapshot, RPC notification).
     */
    using FinalizedCallback = std::function<void(const FinalizedBlockRecord&)>;

    // Callback: called when this node is the proposer for (height, round).
    // Should produce and finalize a block. Returns true if block was produced.
    using BlockProducerCallback = std::function<bool(std::uint64_t height, std::uint64_t round, std::int64_t now)>;

    ConsensusEventLoop(
        node::NodeRuntime&           runtime,
        p2p::GossipMesh&             gossip,
        const crypto::CryptoPolicy&  policy,
        const crypto::SignatureProvider& provider
    );

    ~ConsensusEventLoop();

    /*
     * Optionally register a callback to run whenever a block is finalized.
     * Must be set before start().
     */
    void setFinalizedCallback(FinalizedCallback cb);

    /*
     * Register a callback to invoke when this node is the designated proposer
     * for the current (height, round). The callback should call
     * RuntimeBlockPipeline::produceAndFinalizeNextBlock(). Must be set before
     * start() to enable local block production.
     */
    void setBlockProducerCallback(BlockProducerCallback cb);

    /*
     * Identify this node's validator address so that ProposerSchedule can
     * determine whether local block production is required. Must be set before
     * start() if block production is desired.
     */
    void setLocalValidatorAddress(const std::string& address);

    /*
     * Set local validator signer for BFT prevoting and precommitting.
     */
    void setLocalSigner(const crypto::Signer* signer);

    /*
     * Wire an EvidencePool so that double-votes detected in the VotePool are
     * automatically forwarded as SlashingEvidence. Optional: if not set, double-
     * vote detection is skipped. Must be set before start().
     */
    void setEvidencePool(EvidencePool* pool);

    /*
     * Set the filesystem path where lock/vote state is persisted after each
     * vote. When set, ConsensusRecoveryStore::save() is called automatically
     * after a prevote or precommit is accepted so that the state survives
     * a node restart without violating BFT safety.
     */
    void setRecoveryPath(std::filesystem::path path);

    /*
     * Start the background consensus thread with the given tick interval.
     * No-op if already running.
     */
    void start(std::int64_t tickIntervalMs = DEFAULT_TICK_INTERVAL_MS);

    /*
     * Signal the background thread to stop and wait for it to exit.
     * Safe to call multiple times.
     */
    void stop();

    bool isRunning() const;

    /*
     * Execute one consensus tick synchronously.
     *
     * Steps:
     *   1. Drain VOTE_ANNOUNCE messages from gossip inbox.
     *   2. Submit each decoded vote to ConsensusRoundManager.
     *   3. Attempt to build a QuorumCertificate from the current VotePool.
     *   4. If quorum is formed, finalize the block.
     *   5. Check round timeout and advance round if expired.
     */
    ConsensusTickResult tick(std::int64_t now);

    /*
     * Restore BFT lock/vote state from a persisted ConsensusRoundState.
     * Called during startup after a recovery state is loaded from disk so that
     * this node does not violate the invariant of voting for two different
     * blocks at the same height across restarts.
     */
    void loadFromRecoveryState(const ConsensusRoundState& state);

private:
    node::NodeRuntime&              m_runtime;
    p2p::GossipMesh&                m_gossip;
    const crypto::CryptoPolicy&     m_policy;
    const crypto::SignatureProvider& m_provider;

    FinalizedCallback    m_onFinalized;
    BlockProducerCallback m_blockProducer;
    std::string           m_localValidatorAddress;
    EvidencePool*         m_evidencePool = nullptr;
    const crypto::Signer* m_localSigner = nullptr;

    std::string           m_lockedBlock = "";
    std::uint64_t         m_lockedRound = 0;
    bool                  m_votedPrevote = false;
    bool                  m_votedPrecommit = false;
    std::uint64_t         m_lastProcessedHeight = 0;
    std::optional<std::filesystem::path> m_recoveryPath;

    std::atomic<bool>    m_running;
    std::thread          m_thread;
    std::int64_t         m_tickIntervalMs;

    BlockFinalizationRegistry m_finalizationRegistry;

    void runLoop();

    ConsensusTickResult drainVotesAndCollect(std::int64_t now);
    bool tryFinalizeBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        const std::string& previousHash,
        std::uint64_t round,
        std::int64_t now,
        ConsensusTickResult& result
    );

    // Broadcasts a CHAIN_STATUS message to peers announcing the new round.
    // Called after advanceConsensusRoundIfTimedOut() returns true.
    void broadcastRoundAdvancement(std::int64_t now);
};

} // namespace nodo::consensus

#endif
