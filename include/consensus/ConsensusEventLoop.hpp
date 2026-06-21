#ifndef NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP
#define NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "consensus/VotePool.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/GossipMesh.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
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

private:
    node::NodeRuntime&              m_runtime;
    p2p::GossipMesh&                m_gossip;
    const crypto::CryptoPolicy&     m_policy;
    const crypto::SignatureProvider& m_provider;

    FinalizedCallback    m_onFinalized;
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
};

} // namespace nodo::consensus

#endif
