#ifndef NODO_CONSENSUS_FORK_CHOICE_HPP
#define NODO_CONSENSUS_FORK_CHOICE_HPP

#include "consensus/BlockFinalizer.hpp"
#include "core/Blockchain.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

/*
 * FinalizedCheckpoint is the compact finality anchor used by fork choice and
 * sync. It is intentionally small enough to exchange with peers before asking
 * for full blocks.
 */
class FinalizedCheckpoint {
public:
    FinalizedCheckpoint();

    FinalizedCheckpoint(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string previousHash,
        std::uint64_t round,
        std::int64_t finalizedAt
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& previousHash() const;
    std::uint64_t round() const;
    std::int64_t finalizedAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_previousHash;
    std::uint64_t m_round;
    std::int64_t m_finalizedAt;
};

/*
 * ChainForkSummary is the local/remote chain summary used by fork choice.
 *
 * It does not replace full validation. It is a safe pre-check used before a
 * node decides whether a candidate chain is worth syncing.
 */
class ChainForkSummary {
public:
    ChainForkSummary();

    ChainForkSummary(
        std::size_t chainSize,
        std::uint64_t latestBlockIndex,
        std::string latestBlockHash
    );

    ChainForkSummary(
        std::size_t chainSize,
        std::uint64_t latestBlockIndex,
        std::string latestBlockHash,
        FinalizedCheckpoint finalizedCheckpoint
    );

    std::size_t chainSize() const;
    std::uint64_t latestBlockIndex() const;
    const std::string& latestBlockHash() const;
    bool hasFinalizedCheckpoint() const;
    const FinalizedCheckpoint& finalizedCheckpoint() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::size_t m_chainSize;
    std::uint64_t m_latestBlockIndex;
    std::string m_latestBlockHash;
    bool m_hasFinalizedCheckpoint;
    FinalizedCheckpoint m_finalizedCheckpoint;
};

enum class ForkChoiceDecision {
    KEEP_LOCAL,
    ADOPT_CANDIDATE,
    EQUAL_CHAINS,
    REJECT_CANDIDATE
};

std::string forkChoiceDecisionToString(
    ForkChoiceDecision decision
);

enum class ForkChoiceRejectReason {
    NONE,
    INVALID_LOCAL_CHAIN,
    INVALID_CANDIDATE_CHAIN,
    INVALID_LOCAL_FINALIZATION_REGISTRY,
    INVALID_CANDIDATE_FINALIZATION_REGISTRY,
    CANDIDATE_CONFLICTS_WITH_LOCAL_FINALITY,
    CANDIDATE_BEHIND_LOCAL_FINALITY,
    CANDIDATE_FINALITY_CONFLICT,
    CANDIDATE_NOT_BETTER
};

std::string forkChoiceRejectReasonToString(
    ForkChoiceRejectReason reason
);

class ForkChoiceResult {
public:
    ForkChoiceResult();

    static ForkChoiceResult keepLocal(
        std::string reason
    );

    static ForkChoiceResult adoptCandidate(
        std::string reason
    );

    static ForkChoiceResult equalChains(
        std::string reason
    );

    static ForkChoiceResult rejectCandidate(
        ForkChoiceRejectReason reason,
        std::string detail
    );

    ForkChoiceDecision decision() const;
    ForkChoiceRejectReason rejectReason() const;
    const std::string& detail() const;

    bool shouldAdoptCandidate() const;
    bool rejected() const;

    std::string serialize() const;

private:
    ForkChoiceDecision m_decision;
    ForkChoiceRejectReason m_rejectReason;
    std::string m_detail;
};

/*
 * ForkChoicePolicy chooses whether a candidate chain is better than the local
 * chain without violating finalized checkpoints.
 *
 * Security principle:
 * A node must never adopt a chain that conflicts with a finalized block it
 * already trusts.
 */
class ForkChoicePolicy {
public:
    static ChainForkSummary summarizeChain(
        const core::Blockchain& chain,
        const BlockFinalizationRegistry& finalizationRegistry
    );

    static bool checkpointMatchesChain(
        const core::Blockchain& chain,
        const FinalizedCheckpoint& checkpoint
    );

    static bool candidateContainsLocalFinality(
        const core::Blockchain& candidateChain,
        const ChainForkSummary& localSummary
    );

    static ForkChoiceResult chooseBestChain(
        const core::Blockchain& localChain,
        const BlockFinalizationRegistry& localFinalizationRegistry,
        const core::Blockchain& candidateChain,
        const BlockFinalizationRegistry& candidateFinalizationRegistry
    );
};

} // namespace nodo::consensus

#endif
