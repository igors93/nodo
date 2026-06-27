#ifndef NODO_CONSENSUS_BLOCK_FINALIZATION_PHASE_HPP
#define NODO_CONSENSUS_BLOCK_FINALIZATION_PHASE_HPP

#include "consensus/BlockFinalizer.hpp"
#include "core/Block.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/NodeRuntime.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {
class NodeDataDirectoryConfig;
}

namespace nodo::consensus {

struct BlockFinalizationPhaseResult {
    bool finalized()    const { return m_finalized; }
    bool insufficient() const { return m_insufficient; }
    const FinalizedBlockRecord& record() const { return m_record; }
    const std::string& reason() const { return m_reason; }

    static BlockFinalizationPhaseResult ok(FinalizedBlockRecord record) {
        BlockFinalizationPhaseResult r;
        r.m_finalized = true;
        r.m_record    = std::move(record);
        return r;
    }

    static BlockFinalizationPhaseResult notEnoughVotes() {
        BlockFinalizationPhaseResult r;
        r.m_finalized   = false;
        r.m_insufficient = true;
        return r;
    }

    static BlockFinalizationPhaseResult failed(std::string reason) {
        BlockFinalizationPhaseResult r;
        r.m_finalized    = false;
        r.m_insufficient = false;
        r.m_reason       = std::move(reason);
        return r;
    }

private:
    bool               m_finalized    = false;
    bool               m_insufficient = false;
    FinalizedBlockRecord m_record;
    std::string        m_reason;
};

/*
 * BlockFinalizationPhase — Phase 4 of BFT consensus.
 *
 * Assembles a QuorumCertificate from the PRECOMMIT votes accumulated in the
 * runtime's VotePool and commits the block to the canonical blockchain.
 *
 * Uses the runtime's shared BlockFinalizationRegistry so all subsystems
 * (consensus, sync, daemon) share a consistent view of which heights are
 * finalized. This phase does NOT broadcast the finalized record — the caller
 * is responsible for gossip and application-layer notifications.
 */
class BlockFinalizationPhase {
public:
    static constexpr std::uint64_t QUORUM_NUMERATOR   = 2;
    static constexpr std::uint64_t QUORUM_DENOMINATOR = 3;

    /*
     * Attempt to form a QC and finalize the block.
     *
     * Returns ok() with the FinalizedBlockRecord when 2/3+ of active
     * validator weight has precommitted (PRECOMMIT or APPROVE votes).
     * Returns notEnoughVotes() when quorum is not yet reached — callers
     * should retry on the next tick. Returns failed() for structural errors
     * (duplicate finalization, certificate mismatch, etc.).
     */
    static BlockFinalizationPhaseResult tryFinalize(
        node::NodeRuntime&               runtime,
        const core::Block&               block,
        std::uint64_t                    blockIndex,
        const std::string&               blockHash,
        const std::string&               previousHash,
        std::uint64_t                    round,
        const crypto::CryptoPolicy&      policy,
        const crypto::SignatureProvider& provider,
        std::int64_t                     now,
        const node::NodeDataDirectoryConfig* directoryConfig = nullptr
    );
};

} // namespace nodo::consensus

#endif
