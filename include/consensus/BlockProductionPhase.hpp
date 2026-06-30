#ifndef NODO_CONSENSUS_BLOCK_PRODUCTION_PHASE_HPP
#define NODO_CONSENSUS_BLOCK_PRODUCTION_PHASE_HPP

#include "core/Block.hpp"
#include "consensus/SlashingEvidence.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace nodo::consensus {

/*
 * BlockCandidateResult carries the output of the production phase: either a
 * validated candidate block ready for proposal, or a rejection reason.
 */

struct PendingSlashingEvidenceBatch {
    std::vector<DoubleVoteEvidence> doubleVotes;
    std::vector<ProposerEquivocationEvidence> proposerEquivocations;

    PendingSlashingEvidenceBatch() = default;

    PendingSlashingEvidenceBatch(std::initializer_list<DoubleVoteEvidence> values)
        : doubleVotes(values), proposerEquivocations() {}

    PendingSlashingEvidenceBatch(
        std::vector<DoubleVoteEvidence> doubleVoteEvidence,
        std::vector<ProposerEquivocationEvidence> proposerEquivocationEvidence = {}
    ) : doubleVotes(std::move(doubleVoteEvidence)),
        proposerEquivocations(std::move(proposerEquivocationEvidence)) {}

    bool empty() const {
        return doubleVotes.empty() && proposerEquivocations.empty();
    }

    std::size_t size() const {
        return doubleVotes.size() + proposerEquivocations.size();
    }
};

struct BlockCandidateResult {
    bool produced() const { return m_block.has_value(); }
    const core::Block& block() const { return *m_block; }
    const std::string& reason() const { return m_reason; }

    static BlockCandidateResult ok(core::Block block) {
        BlockCandidateResult r;
        r.m_block = std::move(block);
        return r;
    }

    static BlockCandidateResult failed(std::string reason) {
        BlockCandidateResult r;
        r.m_reason = std::move(reason);
        return r;
    }

private:
    std::optional<core::Block> m_block;
    std::string                m_reason;
};

/*
 * BlockProductionPhase — Phase 1 of BFT consensus.
 *
 * Selects transactions from the mempool, assembles a candidate block, and
 * validates it against the current chain state and monetary policy.
 *
 * This phase has no side effects on the consensus round or vote pool.
 * It does NOT submit votes, build a QC, or finalize anything.
 */
class BlockProductionPhase {
public:
    /*
     * Build and validate a candidate block for the current consensus height.
     *
     * On success the returned result holds the fully validated block.
     * On failure the result carries a human-readable rejection reason.
     */
    static BlockCandidateResult produce(
        node::NodeRuntime&                     runtime,
        const node::RuntimeBlockPipelineConfig& config,
        PendingSlashingEvidenceBatch slashingEvidence = {}
    );
};

} // namespace nodo::consensus

#endif
