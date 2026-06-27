#include "consensus/ConsensusEventLoop.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/Hex.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/DoubleVoteDetector.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <array>
#include <chrono>
#include <thread>

namespace nodo::consensus {

namespace {

static const std::string kCanonicalPayloadPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

} // namespace

ConsensusEventLoop::ConsensusEventLoop(
    node::NodeRuntime&               runtime,
    p2p::GossipMesh&                 gossip,
    const crypto::CryptoPolicy&      policy,
    const crypto::SignatureProvider& provider
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

void ConsensusEventLoop::setBlockProducerCallback(BlockProducerCallback cb) {
    m_blockProducer = std::move(cb);
}

void ConsensusEventLoop::setLocalValidatorAddress(const std::string& address) {
    m_localValidatorAddress = address;
}

void ConsensusEventLoop::setLocalSigner(const crypto::Signer* signer) {
    m_localSigner = signer;
}

void ConsensusEventLoop::setEvidencePool(EvidencePool* pool) {
    m_evidencePool = pool;
}

void ConsensusEventLoop::setRecoveryPath(std::filesystem::path path) {
    m_recoveryPath = std::move(path);
}

void ConsensusEventLoop::loadFromRecoveryState(const ConsensusRoundState& state) {
    m_lockedBlock         = state.lockedBlockHash();
    m_lockedRound         = state.lockedRound();
    m_votedPrevote        = state.votedPrevote();
    m_votedPrecommit      = state.votedPrecommit();
    m_lastProcessedHeight = state.height();
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

    if (m_runtime.blockchain().empty()) return result;

    const core::Blockchain& chain  = m_runtime.blockchain();
    const auto& state              = m_runtime.consensusRoundManager().currentState();
    const std::uint64_t height     = state.height();
    const std::uint64_t round      = state.round();

    // After a restart, the recovery store sets m_lastProcessedHeight to the
    // height at which we last voted. If the finalization registry shows that
    // height already finalized, clear lock/vote flags so the next height starts clean.
    if (height > m_lastProcessedHeight &&
        m_runtime.finalizationRegistry().hasFinalizedHeight(
            m_lastProcessedHeight > 0 ? m_lastProcessedHeight : height)) {
        m_lastProcessedHeight    = height;
        m_lockedBlock            = "";
        m_lockedRound            = 0;
        m_votedPrevote           = false;
        m_votedPrecommit         = false;
        m_producedThisRound      = false;
    } else if (m_lastProcessedHeight == 0 && height > 0) {
        m_lastProcessedHeight = height;
    }

    // -------------------------------------------------------------------------
    // PHASE 1 (PRODUCTION) + PHASE 2 (PROPOSAL)
    //
    // When this node is the designated proposer and hasn't produced a block
    // for the current round yet:
    //   1. Invoke the production callback to build a validated candidate block.
    //   2. Add the block to the local chain (enables same-tick voting).
    //   3. Broadcast it as a signed BLOCK_PROPOSAL for peers to validate.
    //
    // After this block the proposer falls through to the same prevote/precommit
    // path as every other validator — no APPROVE shortcuts.
    // -------------------------------------------------------------------------
    if (!m_localValidatorAddress.empty() && m_blockProducer && !m_producedThisRound) {
        const std::uint64_t tipHeight  = chain.latestBlock().index();
        const std::uint64_t nextHeight = tipHeight + 1;

        if (height == nextHeight) {
            const std::string chainId =
                m_runtime.config().genesisConfig().networkParameters().chainId();
            const std::string proposer = ProposerSchedule::selectProposer(
                m_runtime.validatorRegistry(), chainId, height, round
            );

            if (proposer == m_localValidatorAddress) {
                std::optional<core::Block> blockOpt = m_blockProducer(height, round, now);

                if (blockOpt.has_value()) {
                    m_runtime.mutableBlockchain().addBlock(*blockOpt);
                    m_runtime.applyGovernanceFromBlock(*blockOpt, now);
                    m_runtime.invalidateAccountStateCache();

                    // Phase 2: sign and broadcast the proposal.
                    if (m_localSigner) {
                        BlockProposalPhase::propose(
                            *blockOpt,
                            m_localValidatorAddress,
                            round,
                            now,
                            *m_localSigner,
                            m_gossip,
                            m_provider
                        );
                    }
                }

                m_producedThisRound = true;
            }
        }
    }

    // If no block exists at the current consensus height, there is nothing
    // to vote on. Wait for the proposer's BLOCK_PROPOSAL to arrive.
    if (chain.latestBlock().index() != height) {
        return result;
    }

    const core::Block& tip        = chain.latestBlock();
    const std::string& blockHash  = tip.hash();
    const std::string& prevHash   = tip.previousHash();

    // Count accumulated prevotes for the current block.
    const VotePool& pool = m_runtime.consensusRoundManager().voteCollector().votePool();
    std::vector<ValidatorVoteRecord> currentVotes =
        pool.votesForBlock(height, blockHash, round);

    std::uint64_t prevoteCount = 0;
    for (const auto& v : currentVotes) {
        if (v.decision() == ValidatorVoteDecision::PREVOTE) {
            prevoteCount++;
        }
    }

    const std::uint64_t activeValidators =
        m_runtime.validatorRegistry().activeCount();
    const std::uint64_t requiredVotes =
        QuorumCertificateBuilder::requiredVoteCount(
            activeValidators, QUORUM_NUMERATOR, QUORUM_DENOMINATOR
        );

    // -------------------------------------------------------------------------
    // PHASE 3a (PREVOTE)
    //
    // Cast a PREVOTE if:
    //   - We have not already voted at this height/round.
    //   - We are an active validator.
    //   - The block is safe to vote for (no conflicting lock from a prior round).
    // -------------------------------------------------------------------------
    if (!m_votedPrevote &&
        m_localSigner &&
        m_runtime.validatorRegistry().isActiveValidator(m_localSigner->address())) {

        if (m_lockedBlock.empty() || blockHash == m_lockedBlock || round > m_lockedRound) {
            const VoteCastResult prevoteResult = BlockVotingPhase::castPrevote(
                m_runtime, tip, round, now, *m_localSigner, m_gossip
            );

            if (prevoteResult.cast()) {
                m_votedPrevote = true;
                prevoteCount++;  // Count own vote immediately for same-tick precommit check.
                saveRecoveryState(m_votedPrevote, m_votedPrecommit);
            }
        }
    }

    // -------------------------------------------------------------------------
    // PHASE 3b (PRECOMMIT)
    //
    // Cast a PRECOMMIT once the PREVOTE quorum threshold is reached.
    // This locks this validator to the block at (blockHash, round).
    // -------------------------------------------------------------------------
    if (m_votedPrevote &&
        !m_votedPrecommit &&
        m_localSigner &&
        m_runtime.validatorRegistry().isActiveValidator(m_localSigner->address())) {

        if (prevoteCount >= requiredVotes) {
            m_lockedBlock = blockHash;
            m_lockedRound = round;

            const VoteCastResult precommitResult = BlockVotingPhase::castPrecommit(
                m_runtime, tip, round, now, *m_localSigner, m_gossip
            );

            if (precommitResult.cast()) {
                m_votedPrecommit = true;
                saveRecoveryState(m_votedPrevote, m_votedPrecommit);
            }
        }
    }

    // -------------------------------------------------------------------------
    // PHASE 4 (FINALIZATION)
    //
    // Attempt to assemble a QuorumCertificate and finalize the block.
    // On success:
    //   - Broadcast the FinalizedBlockRecord to peers.
    //   - Advance the consensus round manager to the next height.
    //   - Reset per-height lock/vote state.
    //   - Invoke the registered finalization callback (persistence, etc.).
    // -------------------------------------------------------------------------
    const BlockFinalizationPhaseResult finResult = BlockFinalizationPhase::tryFinalize(
        m_runtime, tip, height, blockHash, prevHash, round, m_policy, m_provider, now
    );

    if (finResult.finalized()) {
        result.quorumFormed      = true;
        result.blockFinalized    = true;
        result.finalizedBlockHash = finResult.record().blockHash();
        result.finalizedHeight   = finResult.record().blockIndex();

        // Broadcast the finalized record to lagging peers.
        m_gossip.broadcast(
            p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT,
            finResult.record().serialize(),
            now
        );

        // Advance the consensus round to the next height.
        const std::uint64_t nextHeight = height + 1;
        const std::string chainId =
            m_runtime.config().genesisConfig().networkParameters().chainId();
        const std::string nextProposer =
            ProposerSchedule::selectProposer(
                m_runtime.validatorRegistry(), chainId, nextHeight, 1
            );
        const std::int64_t targetBlockTimeSec =
            m_runtime.config().genesisConfig().networkParameters().targetBlockTimeSeconds();
        m_runtime.mutableConsensusRoundManager().advanceToHeight(
            nextHeight, 1, nextProposer, now, targetBlockTimeSec
        );

        // Reset per-height BFT state.
        m_lastProcessedHeight = nextHeight;
        m_lockedBlock         = "";
        m_lockedRound         = 0;
        m_votedPrevote        = false;
        m_votedPrecommit      = false;
        m_producedThisRound   = false;

        // Notify application layer (disk persistence, snapshots, etc.).
        if (m_onFinalized) {
            m_onFinalized(finResult.record());
        }
    }

    // Round timeout: advance to the next round at the same height when the
    // proposer has not produced in time.
    if (m_runtime.advanceConsensusRoundIfTimedOut(now)) {
        result.roundAdvanced    = true;
        m_votedPrevote          = false;
        m_votedPrecommit        = false;
        m_producedThisRound     = false;
        broadcastRoundAdvancement(now);
    }

    return result;
}

ConsensusTickResult ConsensusEventLoop::drainVotesAndCollect(std::int64_t now) {
    ConsensusTickResult result;

    const std::array<p2p::NetworkMessageType, 2> voteTypes = {
        p2p::NetworkMessageType::VOTE_ANNOUNCE,
        p2p::NetworkMessageType::VALIDATOR_VOTE
    };

    for (const auto voteType : voteTypes) {
        const auto voteMessages = m_gossip.drainInbox(voteType);

        for (const auto& envelope : voteMessages) {
            if (envelope.payload().empty()) continue;

            try {
                ValidatorVoteRecord vote =
                    ValidatorVoteRecord::deserialize(envelope.payload());

                if (!vote.isStructurallyValid(m_policy)) continue;

                const VoteCollectResult collected =
                    m_runtime.submitConsensusVote(vote);

                if (collected.accepted()) {
                    result.votesCollected++;
                }
            } catch (const std::exception&) {
                continue;
            }
        }
    }

    // Detect double-votes and forward them to the evidence pool for slashing.
    if (m_evidencePool) {
        const VotePool& pool =
            m_runtime.consensusRoundManager().voteCollector().votePool();
        node::DoubleVoteDetector::detect(pool, *m_evidencePool, m_policy, now);
    }

    return result;
}

void ConsensusEventLoop::saveRecoveryState(bool votedPrevote, bool votedPrecommit) {
    if (!m_recoveryPath.has_value()) return;

    const auto& state = m_runtime.consensusRoundManager().currentState();
    ConsensusRoundState updatedState(
        state.height(),
        state.round(),
        state.proposerAddress(),
        state.roundStartedAt(),
        m_lockedBlock,
        m_lockedRound,
        votedPrevote,
        votedPrecommit
    );
    ConsensusRecoveryStore::save(*m_recoveryPath, updatedState);
}

void ConsensusEventLoop::broadcastRoundAdvancement(std::int64_t now) {
    const auto& state = m_runtime.consensusRoundManager().currentState();

    const node::RoundAdvanceMessage message(
        state.height(),
        state.round(),
        state.proposerAddress(),
        now
    );

    if (!message.isValid()) return;

    const std::string payload =
        kCanonicalPayloadPrefix +
        crypto::hexEncode(
            serialization::ProtocolMessageCodec::encodeRoundAdvanceMessage(message)
        );

    m_gossip.broadcast(p2p::NetworkMessageType::CHAIN_STATUS, payload, now);
}

} // namespace nodo::consensus
