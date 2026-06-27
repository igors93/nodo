#include "consensus/BlockFinalizationPhase.hpp"

#include "node/RuntimeBlockPipeline.hpp"

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
    std::int64_t                     now,
    const node::NodeDataDirectoryConfig* directoryConfig
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

    const node::RuntimeBlockPipelineResult commitResult =
        node::RuntimeBlockPipeline::commitCertifiedBlock(
        runtime,
        block,
        qcResult.certificate(),
        now,
        directoryConfig
    );

    if (!commitResult.finalized()) {
        return BlockFinalizationPhaseResult::failed(commitResult.reason());
    }

    return BlockFinalizationPhaseResult::ok(commitResult.finalizedRecord());
}

} // namespace nodo::consensus
