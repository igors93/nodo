#include "consensus/ForkChoice.hpp"

#include <sstream>
#include <utility>

namespace nodo::consensus {

namespace {

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

FinalizedCheckpoint checkpointFromRecord(
    const FinalizedBlockRecord& record
) {
    return FinalizedCheckpoint(
        record.blockIndex(),
        record.blockHash(),
        record.previousHash(),
        record.round(),
        record.finalizedAt()
    );
}

} // namespace

FinalizedCheckpoint::FinalizedCheckpoint()
    : m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_round(0),
      m_finalizedAt(0) {}

FinalizedCheckpoint::FinalizedCheckpoint(
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    std::int64_t finalizedAt
)
    : m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_finalizedAt(finalizedAt) {}

std::uint64_t FinalizedCheckpoint::blockIndex() const {
    return m_blockIndex;
}

const std::string& FinalizedCheckpoint::blockHash() const {
    return m_blockHash;
}

const std::string& FinalizedCheckpoint::previousHash() const {
    return m_previousHash;
}

std::uint64_t FinalizedCheckpoint::round() const {
    return m_round;
}

std::int64_t FinalizedCheckpoint::finalizedAt() const {
    return m_finalizedAt;
}

bool FinalizedCheckpoint::isValid() const {
    return m_blockIndex > 0 &&
           m_round > 0 &&
           m_finalizedAt > 0 &&
           isSafeScalar(m_blockHash) &&
           isSafeScalar(m_previousHash);
}

std::string FinalizedCheckpoint::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedCheckpoint{"
        << "blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";finalizedAt=" << m_finalizedAt
        << "}";

    return oss.str();
}

ChainForkSummary::ChainForkSummary()
    : m_chainSize(0),
      m_latestBlockIndex(0),
      m_latestBlockHash(""),
      m_hasFinalizedCheckpoint(false),
      m_finalizedCheckpoint() {}

ChainForkSummary::ChainForkSummary(
    std::size_t chainSize,
    std::uint64_t latestBlockIndex,
    std::string latestBlockHash
)
    : m_chainSize(chainSize),
      m_latestBlockIndex(latestBlockIndex),
      m_latestBlockHash(std::move(latestBlockHash)),
      m_hasFinalizedCheckpoint(false),
      m_finalizedCheckpoint() {}

ChainForkSummary::ChainForkSummary(
    std::size_t chainSize,
    std::uint64_t latestBlockIndex,
    std::string latestBlockHash,
    FinalizedCheckpoint finalizedCheckpoint
)
    : m_chainSize(chainSize),
      m_latestBlockIndex(latestBlockIndex),
      m_latestBlockHash(std::move(latestBlockHash)),
      m_hasFinalizedCheckpoint(true),
      m_finalizedCheckpoint(std::move(finalizedCheckpoint)) {}

std::size_t ChainForkSummary::chainSize() const {
    return m_chainSize;
}

std::uint64_t ChainForkSummary::latestBlockIndex() const {
    return m_latestBlockIndex;
}

const std::string& ChainForkSummary::latestBlockHash() const {
    return m_latestBlockHash;
}

bool ChainForkSummary::hasFinalizedCheckpoint() const {
    return m_hasFinalizedCheckpoint;
}

const FinalizedCheckpoint& ChainForkSummary::finalizedCheckpoint() const {
    return m_finalizedCheckpoint;
}

bool ChainForkSummary::isValid() const {
    if (m_chainSize == 0 ||
        m_latestBlockHash.empty() ||
        m_latestBlockIndex + 1 != m_chainSize) {
        return false;
    }

    if (!m_hasFinalizedCheckpoint) {
        return true;
    }

    return m_finalizedCheckpoint.isValid() &&
           m_finalizedCheckpoint.blockIndex() <= m_latestBlockIndex;
}

std::string ChainForkSummary::serialize() const {
    std::ostringstream oss;

    oss << "ChainForkSummary{"
        << "chainSize=" << m_chainSize
        << ";latestBlockIndex=" << m_latestBlockIndex
        << ";latestBlockHash=" << m_latestBlockHash
        << ";hasFinalizedCheckpoint=" << (m_hasFinalizedCheckpoint ? "true" : "false")
        << ";finalizedCheckpoint="
        << (m_hasFinalizedCheckpoint ? m_finalizedCheckpoint.serialize() : "NONE")
        << "}";

    return oss.str();
}

std::string forkChoiceDecisionToString(
    ForkChoiceDecision decision
) {
    switch (decision) {
        case ForkChoiceDecision::KEEP_LOCAL:
            return "KEEP_LOCAL";
        case ForkChoiceDecision::ADOPT_CANDIDATE:
            return "ADOPT_CANDIDATE";
        case ForkChoiceDecision::EQUAL_CHAINS:
            return "EQUAL_CHAINS";
        case ForkChoiceDecision::REJECT_CANDIDATE:
            return "REJECT_CANDIDATE";
        default:
            return "REJECT_CANDIDATE";
    }
}

std::string forkChoiceRejectReasonToString(
    ForkChoiceRejectReason reason
) {
    switch (reason) {
        case ForkChoiceRejectReason::NONE:
            return "NONE";
        case ForkChoiceRejectReason::INVALID_LOCAL_CHAIN:
            return "INVALID_LOCAL_CHAIN";
        case ForkChoiceRejectReason::INVALID_CANDIDATE_CHAIN:
            return "INVALID_CANDIDATE_CHAIN";
        case ForkChoiceRejectReason::INVALID_LOCAL_FINALIZATION_REGISTRY:
            return "INVALID_LOCAL_FINALIZATION_REGISTRY";
        case ForkChoiceRejectReason::INVALID_CANDIDATE_FINALIZATION_REGISTRY:
            return "INVALID_CANDIDATE_FINALIZATION_REGISTRY";
        case ForkChoiceRejectReason::CANDIDATE_CONFLICTS_WITH_LOCAL_FINALITY:
            return "CANDIDATE_CONFLICTS_WITH_LOCAL_FINALITY";
        case ForkChoiceRejectReason::CANDIDATE_BEHIND_LOCAL_FINALITY:
            return "CANDIDATE_BEHIND_LOCAL_FINALITY";
        case ForkChoiceRejectReason::CANDIDATE_FINALITY_CONFLICT:
            return "CANDIDATE_FINALITY_CONFLICT";
        case ForkChoiceRejectReason::CANDIDATE_NOT_BETTER:
            return "CANDIDATE_NOT_BETTER";
        default:
            return "CANDIDATE_NOT_BETTER";
    }
}

ForkChoiceResult::ForkChoiceResult()
    : m_decision(ForkChoiceDecision::REJECT_CANDIDATE),
      m_rejectReason(ForkChoiceRejectReason::INVALID_CANDIDATE_CHAIN),
      m_detail("Uninitialized fork choice result.") {}

ForkChoiceResult ForkChoiceResult::keepLocal(
    std::string reason
) {
    ForkChoiceResult result;
    result.m_decision = ForkChoiceDecision::KEEP_LOCAL;
    result.m_rejectReason = ForkChoiceRejectReason::CANDIDATE_NOT_BETTER;
    result.m_detail = std::move(reason);
    return result;
}

ForkChoiceResult ForkChoiceResult::adoptCandidate(
    std::string reason
) {
    ForkChoiceResult result;
    result.m_decision = ForkChoiceDecision::ADOPT_CANDIDATE;
    result.m_rejectReason = ForkChoiceRejectReason::NONE;
    result.m_detail = std::move(reason);
    return result;
}

ForkChoiceResult ForkChoiceResult::equalChains(
    std::string reason
) {
    ForkChoiceResult result;
    result.m_decision = ForkChoiceDecision::EQUAL_CHAINS;
    result.m_rejectReason = ForkChoiceRejectReason::NONE;
    result.m_detail = std::move(reason);
    return result;
}

ForkChoiceResult ForkChoiceResult::rejectCandidate(
    ForkChoiceRejectReason reason,
    std::string detail
) {
    ForkChoiceResult result;
    result.m_decision = ForkChoiceDecision::REJECT_CANDIDATE;
    result.m_rejectReason = reason;
    result.m_detail = std::move(detail);
    return result;
}

ForkChoiceDecision ForkChoiceResult::decision() const {
    return m_decision;
}

ForkChoiceRejectReason ForkChoiceResult::rejectReason() const {
    return m_rejectReason;
}

const std::string& ForkChoiceResult::detail() const {
    return m_detail;
}

bool ForkChoiceResult::shouldAdoptCandidate() const {
    return m_decision == ForkChoiceDecision::ADOPT_CANDIDATE;
}

bool ForkChoiceResult::rejected() const {
    return m_decision == ForkChoiceDecision::REJECT_CANDIDATE;
}

std::string ForkChoiceResult::serialize() const {
    std::ostringstream oss;

    oss << "ForkChoiceResult{"
        << "decision=" << forkChoiceDecisionToString(m_decision)
        << ";rejectReason=" << forkChoiceRejectReasonToString(m_rejectReason)
        << ";detail=" << m_detail
        << "}";

    return oss.str();
}

ChainForkSummary ForkChoicePolicy::summarizeChain(
    const core::Blockchain& chain,
    const BlockFinalizationRegistry& finalizationRegistry
) {
    if (chain.empty() ||
        !chain.isValid() ||
        !finalizationRegistry.isValid()) {
        return ChainForkSummary();
    }

    const std::uint64_t highestFinalizedHeight =
        finalizationRegistry.highestFinalizedHeight();

    if (highestFinalizedHeight == 0) {
        return ChainForkSummary(
            chain.size(),
            chain.latestBlock().index(),
            chain.latestBlock().hash()
        );
    }

    const FinalizedBlockRecord* record =
        finalizationRegistry.recordForHeight(highestFinalizedHeight);

    if (record == nullptr ||
        !record->isStructurallyValid()) {
        return ChainForkSummary();
    }

    const FinalizedCheckpoint checkpoint =
        checkpointFromRecord(*record);

    if (!checkpointMatchesChain(
            chain,
            checkpoint
        )) {
        return ChainForkSummary();
    }

    return ChainForkSummary(
        chain.size(),
        chain.latestBlock().index(),
        chain.latestBlock().hash(),
        checkpoint
    );
}

bool ForkChoicePolicy::checkpointMatchesChain(
    const core::Blockchain& chain,
    const FinalizedCheckpoint& checkpoint
) {
    if (!checkpoint.isValid() ||
        chain.empty() ||
        !chain.isValid()) {
        return false;
    }

    if (checkpoint.blockIndex() >= chain.blocks().size()) {
        return false;
    }

    const core::Block& block =
        chain.blocks()[static_cast<std::size_t>(checkpoint.blockIndex())];

    return block.hash() == checkpoint.blockHash() &&
           block.previousHash() == checkpoint.previousHash();
}

bool ForkChoicePolicy::candidateContainsLocalFinality(
    const core::Blockchain& candidateChain,
    const ChainForkSummary& localSummary
) {
    if (!localSummary.isValid()) {
        return false;
    }

    if (!localSummary.hasFinalizedCheckpoint()) {
        return true;
    }

    return checkpointMatchesChain(
        candidateChain,
        localSummary.finalizedCheckpoint()
    );
}

ForkChoiceResult ForkChoicePolicy::chooseBestChain(
    const core::Blockchain& localChain,
    const BlockFinalizationRegistry& localFinalizationRegistry,
    const core::Blockchain& candidateChain,
    const BlockFinalizationRegistry& candidateFinalizationRegistry
) {
    if (localChain.empty() ||
        !localChain.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_LOCAL_CHAIN,
            "Local chain is empty or invalid."
        );
    }

    if (!localFinalizationRegistry.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_LOCAL_FINALIZATION_REGISTRY,
            "Local finalization registry is invalid."
        );
    }

    if (candidateChain.empty() ||
        !candidateChain.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_CANDIDATE_CHAIN,
            "Candidate chain is empty or invalid."
        );
    }

    if (!candidateFinalizationRegistry.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_CANDIDATE_FINALIZATION_REGISTRY,
            "Candidate finalization registry is invalid."
        );
    }

    const ChainForkSummary localSummary =
        summarizeChain(
            localChain,
            localFinalizationRegistry
        );

    const ChainForkSummary candidateSummary =
        summarizeChain(
            candidateChain,
            candidateFinalizationRegistry
        );

    if (!localSummary.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_LOCAL_CHAIN,
            "Local chain summary is invalid."
        );
    }

    if (!candidateSummary.isValid()) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::INVALID_CANDIDATE_CHAIN,
            "Candidate chain summary is invalid."
        );
    }

    if (!candidateContainsLocalFinality(
            candidateChain,
            localSummary
        )) {
        return ForkChoiceResult::rejectCandidate(
            ForkChoiceRejectReason::CANDIDATE_CONFLICTS_WITH_LOCAL_FINALITY,
            "Candidate chain does not contain local finalized checkpoint."
        );
    }

    if (localSummary.hasFinalizedCheckpoint() &&
        candidateSummary.hasFinalizedCheckpoint()) {
        const FinalizedCheckpoint& localCheckpoint =
            localSummary.finalizedCheckpoint();

        const FinalizedCheckpoint& candidateCheckpoint =
            candidateSummary.finalizedCheckpoint();

        if (candidateCheckpoint.blockIndex() < localCheckpoint.blockIndex()) {
            return ForkChoiceResult::rejectCandidate(
                ForkChoiceRejectReason::CANDIDATE_BEHIND_LOCAL_FINALITY,
                "Candidate finalized checkpoint is behind local finality."
            );
        }

        if (candidateCheckpoint.blockIndex() == localCheckpoint.blockIndex() &&
            candidateCheckpoint.blockHash() != localCheckpoint.blockHash()) {
            return ForkChoiceResult::rejectCandidate(
                ForkChoiceRejectReason::CANDIDATE_FINALITY_CONFLICT,
                "Candidate finality conflicts with local finality at same height."
            );
        }
    }

    if (candidateSummary.hasFinalizedCheckpoint() &&
        (!localSummary.hasFinalizedCheckpoint() ||
         candidateSummary.finalizedCheckpoint().blockIndex() >
             localSummary.finalizedCheckpoint().blockIndex())) {
        return ForkChoiceResult::adoptCandidate(
            "Candidate has higher finalized checkpoint."
        );
    }

    if (candidateSummary.chainSize() > localSummary.chainSize()) {
        return ForkChoiceResult::adoptCandidate(
            "Candidate chain is longer and does not violate finality."
        );
    }

    if (candidateSummary.chainSize() == localSummary.chainSize() &&
        candidateSummary.latestBlockHash() == localSummary.latestBlockHash()) {
        return ForkChoiceResult::equalChains(
            "Candidate chain is identical at the latest block."
        );
    }

    return ForkChoiceResult::keepLocal(
        "Candidate is not better than local chain."
    );
}

} // namespace nodo::consensus
