#ifndef NODO_CONSENSUS_BLOCK_VOTING_PHASE_HPP
#define NODO_CONSENSUS_BLOCK_VOTING_PHASE_HPP

#include "core/Block.hpp"
#include "crypto/Signer.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/GossipMesh.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

struct VoteCastResult {
    bool cast() const { return m_cast; }
    const std::string& reason() const { return m_reason; }

    static VoteCastResult ok() {
        VoteCastResult r;
        r.m_cast = true;
        return r;
    }

    static VoteCastResult failed(std::string reason) {
        VoteCastResult r;
        r.m_cast   = false;
        r.m_reason = std::move(reason);
        return r;
    }

private:
    bool        m_cast = false;
    std::string m_reason;
};

/*
 * BlockVotingPhase — Phase 3 of BFT consensus.
 *
 * Both the proposer and all non-proposing validators go through the same
 * uniform two-step voting sequence:
 *
 *   1. castPrevote  — express intent to commit the block.
 *   2. castPrecommit — lock in once 2/3+ PREVOTE quorum is confirmed.
 *
 * Votes are submitted to the local VotePool and broadcast to peers via gossip.
 * There are no APPROVE votes in this path.
 */
class BlockVotingPhase {
public:
    /*
     * Submit an already signed vote to the local VotePool and broadcast that
     * exact serialized record. This is the recovery-safe entry point: callers
     * may persist the signed vote before exposing it to the network, then call
     * this method now or after restart to rebroadcast the same vote.
     */
    static VoteCastResult submitAndBroadcastSignedVote(
        node::NodeRuntime& runtime,
        const ValidatorVoteRecord& vote,
        std::int64_t now,
        p2p::GossipMesh& gossip
    );

    /*
     * Build a PREVOTE for the given block, submit it to the runtime VotePool,
     * and broadcast it to peers. Returns ok() when the vote was accepted.
     */
    static VoteCastResult castPrevote(
        node::NodeRuntime&    runtime,
        const core::Block&    block,
        std::uint64_t         round,
        std::int64_t          now,
        const crypto::Signer& signer,
        p2p::GossipMesh&      gossip
    );

    /*
     * Build a PRECOMMIT for the given block, submit it to the runtime VotePool,
     * and broadcast it to peers. Should only be called once the PREVOTE quorum
     * threshold has been confirmed by the caller.
     */
    static VoteCastResult castPrecommit(
        node::NodeRuntime&    runtime,
        const core::Block&    block,
        std::uint64_t         round,
        std::int64_t          now,
        const crypto::Signer& signer,
        p2p::GossipMesh&      gossip
    );
};

} // namespace nodo::consensus

#endif
