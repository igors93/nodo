#include "consensus/BlockFinalizationPhase.hpp"

#include "consensus/NetworkVoteCollector.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "consensus/VotePool.hpp"

#include <vector>

namespace nodo::consensus {

BlockFinalizationPhaseResult BlockFinalizationPhase::tryFinalize(
    node::NodeRuntime&               runtime,
    const core::Block&               block,
    std::uint64_t                    blockIndex,
    const std::string&               blockHash,
    const std::string&               previousHash,
    std::uint64_t                    round,
    const crypto::CryptoPolicy&      policy,
    const crypto::SignatureProvider& provider,
    std::int64_t                     now
) {
    // Skip if already finalized at this height.
    if (runtime.finalizationRegistry().hasFinalizedHeight(blockIndex)) {
        return BlockFinalizationPhaseResult::failed(
            "Block at height " + std::to_string(blockIndex) + " already finalized."
        );
    }

    const VotePool& pool =
        runtime.consensusRoundManager().voteCollector().votePool();

    std::vector<ValidatorVoteRecord> allVotes =
        pool.votesForBlock(blockIndex, blockHash, round);

    // Collect PRECOMMIT and APPROVE votes for QC assembly.
    std::vector<ValidatorVoteRecord> certificateVotes;
    certificateVotes.reserve(allVotes.size());
    for (const auto& v : allVotes) {
        if (v.decision() == ValidatorVoteDecision::PRECOMMIT ||
            v.decision() == ValidatorVoteDecision::APPROVE) {
            certificateVotes.push_back(v);
        }
    }

    const QuorumCertificateBuildResult qcResult =
        QuorumCertificateBuilder::buildFromVotes(
            blockIndex,
            blockHash,
            previousHash,
            round,
            certificateVotes,
            runtime.validatorRegistry(),
            policy,
            provider,
            QUORUM_NUMERATOR,
            QUORUM_DENOMINATOR
        );

    if (!qcResult.certified()) {
        return BlockFinalizationPhaseResult::notEnoughVotes();
    }

    BlockFinalizationResult finResult = BlockFinalizer::finalizeBlock(
        runtime.mutableBlockchain(),
        block,
        qcResult.certificate(),
        runtime.validatorRegistry(),
        runtime.mutableFinalizationRegistry(),
        policy,
        provider,
        now
    );

    if (finResult.duplicate()) {
        return BlockFinalizationPhaseResult::failed("Duplicate finalization.");
    }

    if (!finResult.success()) {
        return BlockFinalizationPhaseResult::failed(finResult.reason());
    }

    return BlockFinalizationPhaseResult::ok(finResult.record());
}

} // namespace nodo::consensus
