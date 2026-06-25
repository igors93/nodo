#include "consensus/ConsensusEventLoop.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "consensus/ValidatorVoteBuilder.hpp"
#include "crypto/Hex.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/DoubleVoteDetector.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <array>
#include <chrono>
#include <filesystem>
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
    m_lockedBlock    = state.lockedBlockHash();
    m_lockedRound    = state.lockedRound();
    m_votedPrevote   = state.votedPrevote();
    m_votedPrecommit = state.votedPrecommit();
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

    // Guard: do not access consensus round manager state if the blockchain
    // is empty (genesis not yet committed).
    if (m_runtime.blockchain().empty()) return result;

    const core::Blockchain& chain = m_runtime.blockchain();
    const auto& state = m_runtime.consensusRoundManager().currentState();
    const std::uint64_t height = state.height();
    const std::uint64_t round = state.round();

    // Reset lock/vote state only when a new height is confirmed via finalization.
    // This preserves the BFT safety invariant across restarts: lock state is only
    // cleared when the previous height is actually finalized, not just when the
    // round manager advances to a new height.
    if (height > m_lastProcessedHeight &&
        m_finalizationRegistry.hasFinalizedHeight(
            m_lastProcessedHeight > 0 ? m_lastProcessedHeight : height)) {
        m_lastProcessedHeight = height;
        m_lockedBlock = "";
        m_lockedRound = 0;
        m_votedPrevote = false;
        m_votedPrecommit = false;
    } else if (m_lastProcessedHeight == 0 && height > 0) {
        m_lastProcessedHeight = height;
    }

    // TASK 1: If this node is the designated proposer for the current
    // height+round and no block exists at that height yet, trigger local
    // block production before attempting quorum assembly.
    if (!m_localValidatorAddress.empty() && m_blockProducer) {
        const std::uint64_t tipHeight =
            chain.empty() ? 0 : chain.latestBlock().index();
        const std::uint64_t nextHeight =
            chain.empty() ? 0 : tipHeight + 1;

        // Only produce if the target height is the next expected block.
        if (chain.empty() || height == nextHeight) {
            const std::string chainId =
                m_runtime.config().genesisConfig().networkParameters().chainId();
            const std::string proposer = ProposerSchedule::selectProposer(
                m_runtime.validatorRegistry(),
                chainId,
                height,
                round
            );
            if (proposer == m_localValidatorAddress) {
                m_blockProducer(height, round, now);
            }
        }
    }

    // If no block at the current height is in our blockchain yet, we cannot vote.
    if (chain.latestBlock().index() != height) {
        return result;
    }

    const core::Block& tip = chain.latestBlock();
    const std::string& blockHash = tip.hash();
    const std::string& prevHash  = tip.previousHash();

    const VotePool& pool = m_runtime.consensusRoundManager().voteCollector().votePool();
    std::vector<ValidatorVoteRecord> votes = pool.votesForBlock(height, blockHash, round);

    std::uint64_t prevoteCount = 0;
    std::uint64_t precommitCount = 0;
    for (const auto& v : votes) {
        if (v.decision() == ValidatorVoteDecision::PREVOTE) {
            prevoteCount++;
        } else if (v.decision() == ValidatorVoteDecision::PRECOMMIT) {
            precommitCount++;
        }
    }

    const std::uint64_t activeValidators = m_runtime.validatorRegistry().activeCount();
    const std::uint64_t requiredVotes = QuorumCertificateBuilder::requiredVoteCount(
        activeValidators,
        QUORUM_NUMERATOR,
        QUORUM_DENOMINATOR
    );

    // PREVOTE rule:
    if (!m_votedPrevote && m_localSigner && m_runtime.validatorRegistry().isActiveValidator(m_localSigner->address())) {
        if (m_lockedBlock.empty() || blockHash == m_lockedBlock || round > m_lockedRound) {
            ValidatorVoteRecord prevote = ValidatorVoteBuilder::buildPrevote(
                m_runtime.validatorRegistry(),
                tip,
                round,
                now,
                *m_localSigner
            );
            const auto collected = m_runtime.submitConsensusVote(prevote);
            if (collected.accepted()) {
                m_votedPrevote = true;
                const std::string serializedVote = prevote.serialize();
                m_gossip.broadcast(p2p::NetworkMessageType::VOTE_ANNOUNCE,  serializedVote, now);
                m_gossip.broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE, serializedVote, now);

                if (m_recoveryPath.has_value()) {
                    ConsensusRoundState updatedState(
                        state.height(), state.round(), state.proposerAddress(),
                        state.roundStartedAt(), m_lockedBlock, m_lockedRound,
                        m_votedPrevote, m_votedPrecommit
                    );
                    ConsensusRecoveryStore::save(*m_recoveryPath, updatedState);
                }
            }
        }
    }

    // PRECOMMIT rule:
    if (m_votedPrevote && !m_votedPrecommit && m_localSigner && m_runtime.validatorRegistry().isActiveValidator(m_localSigner->address())) {
        if (prevoteCount >= requiredVotes) {
            m_lockedBlock = blockHash;
            m_lockedRound = round;

            ValidatorVoteRecord precommit = ValidatorVoteBuilder::buildPrecommit(
                m_runtime.validatorRegistry(),
                tip,
                round,
                now,
                *m_localSigner
            );
            const auto collected = m_runtime.submitConsensusVote(precommit);
            if (collected.accepted()) {
                m_votedPrecommit = true;
                const std::string serializedVote = precommit.serialize();
                m_gossip.broadcast(p2p::NetworkMessageType::VOTE_ANNOUNCE,  serializedVote, now);
                m_gossip.broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE, serializedVote, now);

                if (m_recoveryPath.has_value()) {
                    ConsensusRoundState updatedState(
                        state.height(), state.round(), state.proposerAddress(),
                        state.roundStartedAt(), m_lockedBlock, m_lockedRound,
                        m_votedPrevote, m_votedPrecommit
                    );
                    ConsensusRecoveryStore::save(*m_recoveryPath, updatedState);
                }
            }
        }
    }

    if (tryFinalizeBlock(height, blockHash, prevHash, round, now, result)) {
        if (m_onFinalized && result.blockFinalized) {
            const FinalizedBlockRecord* rec =
                m_finalizationRegistry.recordForHeight(height);
            if (rec) m_onFinalized(*rec);
        }
    }

    // TASK 2: Check round timeout, advance if expired, and broadcast to peers.
    if (m_runtime.advanceConsensusRoundIfTimedOut(now)) {
        result.roundAdvanced = true;
        m_votedPrevote = false;
        m_votedPrecommit = false;
        broadcastRoundAdvancement(now);
    }

    return result;
}

ConsensusTickResult ConsensusEventLoop::drainVotesAndCollect(
    std::int64_t now
) {
    ConsensusTickResult result;

    const auto& inbox = m_gossip.inbox();

    // Process both VOTE_ANNOUNCE (legacy) and VALIDATOR_VOTE (new) message types.
    const std::array<p2p::NetworkMessageType, 2> voteTypes = {
        p2p::NetworkMessageType::VOTE_ANNOUNCE,
        p2p::NetworkMessageType::VALIDATOR_VOTE
    };

    for (const auto voteType : voteTypes) {
        const auto voteMessages = inbox.messagesForType(voteType);

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
    }

    // TASK 3: After collecting votes, scan for double-votes and forward them
    // to the EvidencePool as slashing evidence.
    if (m_evidencePool) {
        const VotePool& pool =
            m_runtime.consensusRoundManager().voteCollector().votePool();
        node::DoubleVoteDetector::detect(pool, *m_evidencePool, m_policy, now);
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

    // Filter to PRECOMMIT and APPROVE votes only
    std::vector<ValidatorVoteRecord> allVotes = pool.votesForBlock(blockIndex, blockHash, round);
    std::vector<ValidatorVoteRecord> certificateVotes;
    for (const auto& vote : allVotes) {
        if (vote.decision() == ValidatorVoteDecision::PRECOMMIT || vote.decision() == ValidatorVoteDecision::APPROVE) {
            certificateVotes.push_back(vote);
        }
    }

    const QuorumCertificateBuildResult qcResult =
        QuorumCertificateBuilder::buildFromVotes(
            blockIndex,
            blockHash,
            previousHash,
            round,
            certificateVotes,
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

    // Broadcast the finalized record so lagging peers can learn about it.
    m_gossip.broadcast(
        p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT,
        finResult.record().serialize(),
        now
    );

    return true;
}

void ConsensusEventLoop::broadcastRoundAdvancement(std::int64_t now) {
    const auto& state = m_runtime.consensusRoundManager().currentState();

    const node::RoundAdvanceMessage message(
        state.height(),
        state.round(),
        state.proposerAddress(),
        now
    );

    if (!message.isValid()) {
        return;
    }

    const std::string payload =
        kCanonicalPayloadPrefix +
        crypto::hexEncode(
            serialization::ProtocolMessageCodec::encodeRoundAdvanceMessage(
                message
            )
        );

    m_gossip.broadcast(
        p2p::NetworkMessageType::CHAIN_STATUS,
        payload,
        now
    );
}

} // namespace nodo::consensus
