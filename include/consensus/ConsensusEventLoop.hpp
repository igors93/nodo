#ifndef NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP
#define NODO_CONSENSUS_CONSENSUS_EVENT_LOOP_HPP

#include "consensus/BlockFinalizationPhase.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockProposalPhase.hpp"
#include "consensus/BlockVotingPhase.hpp"
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
#include "node/SlashingEvidenceGossipAdmission.hpp"
#include "node/SlashingEvidenceSync.hpp"
#include "node/SignedBlockProposalMessage.hpp"
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
    std::uint32_t evidenceAccepted = 0;
    std::uint32_t evidenceRejected = 0;
    std::uint32_t evidenceRateLimited = 0;
    std::uint32_t evidenceSyncRequestsSent = 0;
    std::uint32_t evidenceSyncResponsesSent = 0;
    bool          quorumFormed    = false;
    bool          blockFinalized  = false;
    bool          roundAdvanced   = false;
    std::string   finalizedBlockHash;
    std::uint64_t finalizedHeight = 0;
    std::string   errorMessage;

    bool hasError() const { return !errorMessage.empty(); }
};

/*
 * ConsensusEventLoop drives the four BFT phases — production, proposal,
 * voting, and finalization — from a single background thread.
 *
 * Architecture (clean phase separation):
 *
 *   Phase 1 — Production (BlockProductionPhase):
 *     When this node is the designated proposer, builds a validated candidate
 *     block from the mempool.
 *
 *   Phase 2 — Proposal (BlockProposalPhase):
 *     Signs the candidate and broadcasts it as BLOCK_PROPOSAL so peers can
 *     validate and cast their votes.
 *
 *   Phase 3 — Voting (BlockVotingPhase):
 *     BOTH the proposer and non-proposing validators cast the same two-step
 *     PREVOTE → PRECOMMIT sequence. No APPROVE shortcut.
 *
 *   Phase 4 — Finalization (BlockFinalizationPhase):
 *     Once 2/3+ PRECOMMIT weight is reached, assembles a QuorumCertificate
 *     and commits the block. Advances the consensus round to the next height
 *     and notifies registered application callbacks.
 *
 * Thread safety:
 *   The background thread calls tick() in a tight loop with a sleep between
 *   iterations. Callers must not call tick() concurrently with the thread.
 *   To run single-threaded (e.g., tests), call stop() first.
 */
class ConsensusEventLoop {
public:
    static constexpr std::int64_t  DEFAULT_TICK_INTERVAL_MS  = 100;
    static constexpr std::uint64_t QUORUM_NUMERATOR          = 2;
    static constexpr std::uint64_t QUORUM_DENOMINATOR        = 3;

    /*
     * Called when a block is successfully finalized. Can be used to notify
     * application layers (persistence, mempool pruning, RPC notification).
     */
    using FinalizedCallback = std::function<void(const FinalizedBlockRecord&)>;

    /*
     * Called when this node is the proposer for (height, round).
     * Should build and validate a candidate block using BlockProductionPhase.
     * Returns the validated Block, or nullopt if production failed.
     * The event loop retains the block as a round-scoped candidate, broadcasts
     * the proposal, collects votes, and appends it only during finalization.
     */
    using BlockProducerCallback =
        std::function<std::optional<core::Block>(std::uint64_t height,
                                                 std::uint64_t round,
                                                 std::int64_t  now)>;

    ConsensusEventLoop(
        node::NodeRuntime&               runtime,
        p2p::GossipMesh&                 gossip,
        const crypto::CryptoPolicy&      policy,
        const crypto::SignatureProvider& provider
    );

    ~ConsensusEventLoop();

    /*
     * Register a callback invoked whenever a block is finalized.
     * Must be set before start().
     */
    void setFinalizedCallback(FinalizedCallback cb);

    /*
     * Register a callback invoked when this node is the designated proposer
     * for the current (height, round). The callback should call
     * BlockProductionPhase::produce() and return the result block.
     * Must be set before start() to enable local block production.
     */
    void setBlockProducerCallback(BlockProducerCallback cb);

    /*
     * Identify this node's validator address so that ProposerSchedule can
     * determine whether local block production is required.
     */
    void setLocalValidatorAddress(const std::string& address);

    /*
     * Set the local validator signer for BFT prevoting and precommitting.
     */
    void setLocalSigner(const crypto::Signer* signer);

    /*
     * Wire an EvidencePool so that double-votes are forwarded as
     * SlashingEvidence. Optional. Must be set before start().
     */
    void setEvidencePool(EvidencePool* pool);

    /*
     * Set the filesystem path where lock/vote state is persisted after each
     * vote for BFT safety across restarts.
     */
    void setRecoveryPath(std::filesystem::path path);

    // Enable the same durable canonical commit used by the local CLI path.
    void setDataDirectoryConfig(
        const node::NodeDataDirectoryConfig* directoryConfig
    );

    /*
     * Start the background consensus thread.
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
     *   1. Admit live and synchronized slashing evidence from gossip.
     *   2. Drain VOTE_ANNOUNCE / VALIDATOR_VOTE messages from gossip inbox.
     *   3. Forward newly detected double-votes to EvidencePool if wired.
     *   4. Phase 1+2: If proposer, produce candidate + broadcast proposal.
     *   5. Phase 3a: Cast PREVOTE if eligible and not yet voted.
     *   6. Phase 3b: Cast PRECOMMIT if PREVOTE quorum is reached.
     *   7. Phase 4: Try to assemble QC and finalize.
     *   8. Check round timeout and advance round if expired.
     */
    ConsensusTickResult tick(std::int64_t now);

    /*
     * Restore BFT lock/vote state from a persisted ConsensusRoundState.
     * Called during startup to avoid double-voting after restart.
     */
    void loadFromRecoveryState(const ConsensusRoundState& state);

private:
    node::NodeRuntime&               m_runtime;
    p2p::GossipMesh&                 m_gossip;
    const crypto::CryptoPolicy&      m_policy;
    const crypto::SignatureProvider& m_provider;

    FinalizedCallback     m_onFinalized;
    BlockProducerCallback m_blockProducer;
    std::string           m_localValidatorAddress;
    EvidencePool*         m_evidencePool  = nullptr;
    node::SlashingEvidenceGossipAdmission m_evidenceGossipAdmission;
    node::SlashingEvidenceSync m_evidenceSync;
    const crypto::Signer* m_localSigner   = nullptr;

    // BFT lock/vote state — reset when a new height is confirmed.
    std::string           m_lockedBlock   = "";
    std::uint64_t         m_lockedRound   = 0;
    bool                  m_votedPrevote  = false;
    bool                  m_votedPrecommit = false;
    std::optional<ValidatorVoteRecord> m_persistedPrevote;
    std::optional<ValidatorVoteRecord> m_persistedPrecommit;
    bool                  m_rebroadcastedPrevote = false;
    bool                  m_rebroadcastedPrecommit = false;
    bool                  m_producedThisRound = false;
    std::uint64_t         m_lastProcessedHeight = 0;

    struct PendingBlockCandidate {
        core::Block block;
        std::uint64_t round;
        node::SignedBlockProposalMessage proposal;
    };

    // A proposal is not canonical state. It remains here until a valid quorum
    // certificate authorizes BlockFinalizer to append it to the blockchain.
    std::optional<PendingBlockCandidate> m_pendingCandidate;

    std::optional<std::filesystem::path> m_recoveryPath;
    const node::NodeDataDirectoryConfig* m_dataDirectoryConfig = nullptr;

    std::atomic<bool> m_running;
    std::thread       m_thread;
    std::int64_t      m_tickIntervalMs;

    void runLoop();

    ConsensusTickResult drainVotesAndCollect(std::int64_t now);

    void drainSlashingEvidence(
        std::int64_t now,
        ConsensusTickResult& result
    );

    void broadcastSlashingEvidence(
        const DoubleVoteEvidence& evidence,
        std::int64_t now
    );

    void broadcastSlashingEvidence(
        const ProposerEquivocationEvidence& evidence,
        std::int64_t now
    );

    void admitAndBroadcastProposerEquivocationEvidence(
        const ProposerEquivocationEvidence& evidence,
        std::int64_t now,
        ConsensusTickResult& result
    );

    // Validate proposals on the consensus thread and retain at most one
    // candidate for the active height and round.
    void processBlockProposals(ConsensusTickResult& result);

    // Advance a timed-out round even when no proposal was received. Any
    // candidate from the expired round is discarded before the next proposer.
    bool advanceRoundIfTimedOut(std::int64_t now, ConsensusTickResult& result);

    // Persist BFT recovery state containing the exact signed votes before any
    // vote is exposed to the network. Booleans are derived from vote presence.
    bool saveRecoveryState();

    bool persistedVoteMatchesCandidate(
        const ValidatorVoteRecord& vote,
        const core::Block& candidate,
        std::uint64_t round,
        ValidatorVoteDecision decision
    ) const;

    void rebroadcastPersistedVotesForCandidate(
        const core::Block& candidate,
        std::uint64_t round,
        std::int64_t now,
        ConsensusTickResult& result
    );

    // Broadcast a CHAIN_STATUS message after a round advance.
    void broadcastRoundAdvancement(std::int64_t now);
};

} // namespace nodo::consensus

#endif
