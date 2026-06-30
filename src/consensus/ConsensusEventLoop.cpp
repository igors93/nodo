#include "consensus/ConsensusEventLoop.hpp"

#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "crypto/Hex.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/DoubleVoteDetector.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <array>
#include <chrono>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>

namespace nodo::consensus {

namespace {

static const std::string kCanonicalPayloadPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

std::int64_t effectiveMinimumFee(const node::NodeRuntime& runtime) {
    const std::uint64_t raw = runtime.effectiveMinimumFeeRawUnits();
    if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(raw);
}

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

void ConsensusEventLoop::setDataDirectoryConfig(
    const node::NodeDataDirectoryConfig* directoryConfig
) {
    m_dataDirectoryConfig = directoryConfig;
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
    if (tickIntervalMs <= 0) {
        throw std::invalid_argument(
            "Consensus tick interval must be positive."
        );
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_tickIntervalMs = tickIntervalMs;
    m_running.store(true);
    try {
        m_thread = std::thread([this]{ runLoop(); });
    } catch (...) {
        m_running.store(false);
        throw;
    }
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

        try {
            tick(static_cast<std::int64_t>(now));
        } catch (...) {
            m_running.store(false);
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(m_tickIntervalMs)
        );
    }
}

ConsensusTickResult ConsensusEventLoop::tick(std::int64_t now) {
    ConsensusTickResult result = drainVotesAndCollect(now);

    if (m_runtime.blockchain().empty()) return result;

    // BLOCK_PROPOSAL is consensus input. Keeping proposal admission on this
    // thread prevents the daemon from mutating the canonical chain concurrently.
    processBlockProposals();

    const core::Blockchain& chain  = m_runtime.blockchain();
    const auto& state              = m_runtime.consensusRoundManager().currentState();
    const std::uint64_t height     = state.height();
    const std::uint64_t round      = state.round();

    if (!m_runtime.validatorSetHistory().hasSet(height)) {
        result.errorMessage =
            "Validator set history is missing for consensus height " +
            std::to_string(height) + ".";
        return result;
    }
    const core::ValidatorRegistry& validators =
        m_runtime.validatorSetHistory().setAt(height);

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
        m_pendingCandidate.reset();
    } else if (m_lastProcessedHeight == 0 && height > 0) {
        m_lastProcessedHeight = height;
    }

    // -------------------------------------------------------------------------
    // (PRODUCTION) + PHASE 2 (PROPOSAL)
    //
    // When this node is the designated proposer and hasn't produced a block
    // for the current round yet:
    // Invoke the production callback to build a validated candidate block.
    // Broadcast it as a signed BLOCK_PROPOSAL for peers to validate.
    // Retain it outside the canonical chain until PRECOMMIT quorum.
    //
    // After this block the proposer falls through to the same prevote/precommit
    // path as every other validator — no APPROVE shortcuts.
    // -------------------------------------------------------------------------
    if (!m_localValidatorAddress.empty() &&
        m_blockProducer &&
        !m_producedThisRound &&
        !m_pendingCandidate.has_value()) {
        const std::uint64_t tipHeight  = chain.latestBlock().index();
        const std::uint64_t nextHeight = tipHeight + 1;

        if (height == nextHeight) {
            const std::string chainId =
                m_runtime.config().genesisConfig().networkParameters().chainId();
            const std::string proposer = ProposerSchedule::selectProposer(
                validators, chainId, height, round
            );

            if (proposer == m_localValidatorAddress) {
                std::optional<core::Block> blockOpt = m_blockProducer(height, round, now);

                if (blockOpt.has_value()) {
                    // Sign and broadcast the proposal.
                    if (m_localSigner) {
                        const BlockProposalResult proposalResult = BlockProposalPhase::propose(
                            *blockOpt,
                            m_localValidatorAddress,
                            round,
                            now,
                            *m_localSigner,
                            m_gossip,
                            m_provider
                        );

                        if (proposalResult.proposed()) {
                            m_pendingCandidate = PendingBlockCandidate{*blockOpt, round};
                        } else {
                            result.errorMessage = proposalResult.reason();
                        }
                    }
                }

                // An empty mempool or a failed proposal broadcast must not
                // consume the round. Retry until a proposal is retained, or
                // until the round timeout advances.
                m_producedThisRound = m_pendingCandidate.has_value();
            }
        }
    }

    // A missing proposal must not bypass the timeout check; otherwise an
    // offline proposer would permanently stall this height.
    if (!m_pendingCandidate.has_value() ||
        m_pendingCandidate->block.index() != height ||
        m_pendingCandidate->round != round) {
        advanceRoundIfTimedOut(now, result);
        return result;
    }

    const core::Block& candidate = m_pendingCandidate->block;
    const std::string& blockHash = candidate.hash();
    const std::string& prevHash  = candidate.previousHash();

    // Count accumulated prevotes for the current block.
    const VotePool& pool = m_runtime.consensusRoundManager().voteCollector().votePool();
    std::vector<ValidatorVoteRecord> currentVotes =
        pool.votesForBlock(height, blockHash, round);

    std::uint64_t prevoteWeight = 0;
    std::set<std::string> prevoteVoters;
    for (const auto& v : currentVotes) {
        if (v.decision() == ValidatorVoteDecision::PREVOTE &&
            prevoteVoters.insert(v.validatorAddress()).second) {
            const std::uint64_t voterWeight =
                validators.consensusWeightFor(v.validatorAddress());
            if (std::numeric_limits<std::uint64_t>::max() - prevoteWeight < voterWeight) {
                throw std::overflow_error("Prevote weight overflow.");
            }
            prevoteWeight += voterWeight;
        }
    }

    const std::uint64_t totalVotingWeight =
        validators.totalConsensusWeight();
    if (totalVotingWeight == 0) {
        result.errorMessage = "No weighted validators are available for consensus.";
        return result;
    }
    const std::uint64_t requiredWeight =
        QuorumCertificateBuilder::requiredVotingWeight(
            totalVotingWeight,
            m_runtime.config().genesisConfig().networkParameters()
                .quorumThresholdNumerator(),
            m_runtime.config().genesisConfig().networkParameters()
                .quorumThresholdDenominator()
        );

    // -------------------------------------------------------------------------
    // A (PREVOTE)
    //
    // Cast a PREVOTE if:
    //   - We have not already voted at this height/round.
    //   - We are an active validator.
    //   - The block is safe to vote for (no conflicting lock from a prior round).
    // -------------------------------------------------------------------------
    if (!m_votedPrevote &&
        m_localSigner &&
        validators.isEligibleForConsensus(m_localSigner->address())) {

        if (m_lockedBlock.empty() || blockHash == m_lockedBlock || round > m_lockedRound) {
            if (!saveRecoveryState(true, m_votedPrecommit)) {
                result.errorMessage =
                    "Unable to persist PREVOTE safety state; vote was not cast.";
            } else {
                m_votedPrevote = true;
                const VoteCastResult prevoteResult = BlockVotingPhase::castPrevote(
                    m_runtime, candidate, round, now, *m_localSigner, m_gossip
                );
                if (prevoteResult.cast()) {
                    const std::uint64_t ownWeight =
                        validators.consensusWeightFor(m_localSigner->address());
                    if (std::numeric_limits<std::uint64_t>::max() - prevoteWeight < ownWeight) {
                        throw std::overflow_error("Prevote weight overflow.");
                    }
                    prevoteWeight += ownWeight;
                } else {
                    result.errorMessage = prevoteResult.reason();
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // B (PRECOMMIT)
    //
    // Cast a PRECOMMIT once the PREVOTE quorum threshold is reached.
    // This locks this validator to the block at (blockHash, round).
    // -------------------------------------------------------------------------
    if (m_votedPrevote &&
        !m_votedPrecommit &&
        m_localSigner &&
        validators.isEligibleForConsensus(m_localSigner->address())) {

        if (prevoteWeight >= requiredWeight) {
            const std::string previousLockedBlock = m_lockedBlock;
            const std::uint64_t previousLockedRound = m_lockedRound;
            m_lockedBlock = blockHash;
            m_lockedRound = round;

            if (!saveRecoveryState(m_votedPrevote, true)) {
                m_lockedBlock = previousLockedBlock;
                m_lockedRound = previousLockedRound;
                result.errorMessage =
                    "Unable to persist PRECOMMIT safety state; vote was not cast.";
            } else {
                m_votedPrecommit = true;
                const VoteCastResult precommitResult = BlockVotingPhase::castPrecommit(
                    m_runtime, candidate, round, now, *m_localSigner, m_gossip
                );
                if (!precommitResult.cast()) {
                    result.errorMessage = precommitResult.reason();
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // (FINALIZATION)
    //
    // Attempt to assemble a QuorumCertificate and finalize the block.
    // On success:
    //   - Broadcast the FinalizedBlockRecord to peers.
    //   - Advance the consensus round manager to the next height.
    //   - Reset per-height lock/vote state.
    //   - Invoke the registered notification callback.
    // -------------------------------------------------------------------------
    const BlockFinalizationPhaseResult finResult = BlockFinalizationPhase::tryFinalize(
        m_runtime,
        candidate,
        height,
        blockHash,
        prevHash,
        round,
        m_policy,
        m_provider,
        now,
        m_dataDirectoryConfig
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

        const std::uint64_t nextHeight = height + 1;
        // Reset per-height BFT state.
        m_lastProcessedHeight = nextHeight;
        m_lockedBlock         = "";
        m_lockedRound         = 0;
        m_votedPrevote        = false;
        m_votedPrecommit      = false;
        m_producedThisRound   = false;
        m_pendingCandidate.reset();

        // Notify application layer (disk persistence, snapshots, etc.).
        if (m_onFinalized) {
            m_onFinalized(finResult.record());
        }
    }

    advanceRoundIfTimedOut(now, result);

    return result;
}

ConsensusTickResult ConsensusEventLoop::drainVotesAndCollect(std::int64_t now) {
    ConsensusTickResult result;

    drainSlashingEvidence(now, result);

    if (m_evidencePool != nullptr && !m_runtime.blockchain().empty()) {
        const node::SlashingEvidenceSyncResult sync =
            m_evidenceSync.tick(
                m_gossip,
                *m_evidencePool,
                m_runtime.consensusRoundManager().currentState().height(),
                m_runtime.validatorSetHistory(),
                m_policy,
                m_provider,
                now
            );
        result.evidenceAccepted += sync.evidenceAccepted;
        result.evidenceRejected += sync.rejectedMessages;
        result.evidenceRateLimited += sync.rateLimitedMessages;
        result.evidenceSyncRequestsSent += sync.requestsSent;
        result.evidenceSyncResponsesSent += sync.responsesSent;
    }

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
        const node::DoubleVoteDetectionResult detected =
            node::DoubleVoteDetector::detect(
                pool, *m_evidencePool, m_policy, now
            );
        for (const std::string& evidenceId : detected.newEvidenceIds) {
            const DoubleVoteEvidence* evidence =
                m_evidencePool->doubleVoteEvidenceById(evidenceId);
            if (evidence != nullptr) {
                broadcastSlashingEvidence(*evidence, now);
            }
        }
    }

    return result;
}

void ConsensusEventLoop::drainSlashingEvidence(
    std::int64_t now,
    ConsensusTickResult& result
) {
    const auto messages = m_gossip.drainInbox(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE
    );
    if (messages.empty()) return;

    if (m_evidencePool == nullptr || m_runtime.blockchain().empty()) {
        result.evidenceRejected +=
            static_cast<std::uint32_t>(messages.size());
        return;
    }

    const std::uint64_t currentHeight =
        m_runtime.consensusRoundManager().currentState().height();
    for (const p2p::NetworkEnvelope& envelope : messages) {
        const node::SlashingEvidenceGossipResult admitted =
            m_evidenceGossipAdmission.admit(
                envelope,
                m_gossip.config().networkId(),
                m_gossip.config().chainId(),
                currentHeight,
                m_runtime.validatorSetHistory(),
                m_policy,
                m_provider,
                *m_evidencePool,
                now
            );
        if (admitted.rateLimited()) {
            ++result.evidenceRateLimited;
            continue;
        }
        if (!admitted.accepted()) {
            if (!admitted.duplicate()) {
                ++result.evidenceRejected;
            }
            continue;
        }

        ++result.evidenceAccepted;
        const DoubleVoteEvidence* evidence =
            m_evidencePool->doubleVoteEvidenceById(admitted.evidenceId());
        if (evidence != nullptr) {
            broadcastSlashingEvidence(*evidence, now);
        }
    }
}

void ConsensusEventLoop::broadcastSlashingEvidence(
    const DoubleVoteEvidence& evidence,
    std::int64_t now
) {
    const node::SlashingEvidenceAnnouncement announcement(
        m_gossip.config().networkId(),
        m_gossip.config().chainId(),
        m_gossip.config().localNodeId(),
        evidence,
        now
    );
    if (!announcement.isValid()) return;

    m_gossip.broadcast(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
        announcement.serialize(),
        now
    );
}

void ConsensusEventLoop::processBlockProposals() {
    const auto messages = m_gossip.drainInbox(
        p2p::NetworkMessageType::BLOCK_PROPOSAL
    );

    if (messages.empty() || m_runtime.blockchain().empty()) return;

    const core::Blockchain& chain = m_runtime.blockchain();
    const ConsensusRoundState& state =
        m_runtime.consensusRoundManager().currentState();

    if (!m_runtime.validatorSetHistory().hasSet(state.height())) return;
    const core::ValidatorRegistry& validators =
        m_runtime.validatorSetHistory().setAt(state.height());

    if (validators.totalConsensusWeight() == 0) return;

    const std::string chainId =
        m_runtime.config().genesisConfig().networkParameters().chainId();
    const std::string expectedProposer = ProposerSchedule::selectProposer(
        validators,
        chainId,
        state.height(),
        state.round()
    );

    if (expectedProposer.empty()) return;

    for (const auto& envelope : messages) {
        if (envelope.payload().empty()) continue;

        try {
            const node::SignedBlockProposalMessage proposal =
                node::SignedBlockProposalMessage::deserialize(envelope.payload());

            if (!proposal.isValid() ||
                proposal.blockIndex() != state.height() ||
                proposal.round() != state.round()) {
                continue;
            }

            if (!proposal.verify(
                    expectedProposer,
                    validators,
                    m_policy,
                    m_provider)) {
                continue;
            }

            const core::Block block =
                serialization::BlockCodec::deserialize(proposal.serializedBlock());

            if (!block.isValid() ||
                block.index() != proposal.blockIndex() ||
                block.hash() != proposal.blockHash() ||
                !chain.canAppendBlock(block)) {
                continue;
            }

            const core::StateTransitionPreviewContext validationContext =
                node::RuntimeAccountStateBuilder::previewContextAtTip(
                    m_runtime, effectiveMinimumFee(m_runtime)
                );

            // Votes are cast only through the authoritative protocol
            // execution context. Signature verification alone is not enough:
            // account-state enforcement and the canonical protocol-domain
            // executor must also be present before the transition gate runs.
            if (!validationContext.isAuthoritativeProtocolContext()) {
                continue;
            }

            const core::BlockValidationResult validation =
                core::BlockStateTransitionValidator::validateCandidateBlock(
                    chain,
                    block,
                    validationContext
                );

            if (!validation.accepted()) continue;

            if (m_pendingCandidate.has_value()) {
                // Exact duplicates are harmless. A second hash for the same
                // proposer/round is conflicting input and must not replace the
                // candidate already selected for local voting.
                if (m_pendingCandidate->round == state.round() &&
                    m_pendingCandidate->block.index() == state.height()) {
                    continue;
                }
            }

            m_pendingCandidate = PendingBlockCandidate{block, state.round()};
        } catch (const std::exception&) {
            // Malformed or unverifiable peer input is ignored without changing
            // the active candidate or canonical chain.
            continue;
        }
    }
}

bool ConsensusEventLoop::advanceRoundIfTimedOut(
    std::int64_t now,
    ConsensusTickResult& result
) {
    if (!m_runtime.advanceConsensusRoundIfTimedOut(now)) return false;

    result.roundAdvanced    = true;
    m_votedPrevote          = false;
    m_votedPrecommit        = false;
    m_producedThisRound     = false;
    m_pendingCandidate.reset();
    broadcastRoundAdvancement(now);
    return true;
}

bool ConsensusEventLoop::saveRecoveryState(bool votedPrevote, bool votedPrecommit) {
    if (!m_recoveryPath.has_value()) return true;

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
    return ConsensusRecoveryStore::save(*m_recoveryPath, updatedState);
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
