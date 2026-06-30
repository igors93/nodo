#include "consensus/BlockVotingPhase.hpp"

#include "consensus/ValidatorVoteBuilder.hpp"
#include "consensus/NetworkVoteCollector.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <stdexcept>

namespace nodo::consensus {

namespace {

const core::ValidatorRegistry& validatorSetForBlock(
    const node::NodeRuntime& runtime,
    const core::Block& block
) {
    if (!runtime.validatorSetHistory().hasSet(block.index())) {
        throw std::runtime_error(
            "Validator set history is missing for vote height."
        );
    }
    return runtime.validatorSetHistory().setAt(block.index());
}

} // namespace

VoteCastResult BlockVotingPhase::submitAndBroadcastSignedVote(
    node::NodeRuntime& runtime,
    const ValidatorVoteRecord& vote,
    std::int64_t now,
    p2p::GossipMesh& gossip
) {
    try {
        const VoteCollectResult collected = runtime.submitConsensusVote(vote);
        if (!collected.accepted() &&
            collected.status() != VoteCollectStatus::REJECTED_REPLAY) {
            return VoteCastResult::failed(
                validatorVoteDecisionToString(vote.decision()) +
                " rejected by VotePool: " + collected.reason()
            );
        }

        const std::string serialized = vote.serialize();
        gossip.broadcast(p2p::NetworkMessageType::VOTE_ANNOUNCE,  serialized, now);
        gossip.broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE, serialized, now);

        return VoteCastResult::ok();
    } catch (const std::exception& e) {
        return VoteCastResult::failed(
            std::string("Exception during signed vote broadcast: ") + e.what()
        );
    }
}

VoteCastResult BlockVotingPhase::castPrevote(
    node::NodeRuntime&    runtime,
    const core::Block&    block,
    std::uint64_t         round,
    std::int64_t          now,
    const crypto::Signer& signer,
    p2p::GossipMesh&      gossip
) {
    try {
        ValidatorVoteRecord prevote = ValidatorVoteBuilder::buildPrevote(
            validatorSetForBlock(runtime, block),
            block,
            round,
            now,
            signer
        );

        return submitAndBroadcastSignedVote(runtime, prevote, now, gossip);
    } catch (const std::exception& e) {
        return VoteCastResult::failed(
            std::string("Exception during PREVOTE: ") + e.what()
        );
    }
}

VoteCastResult BlockVotingPhase::castPrecommit(
    node::NodeRuntime&    runtime,
    const core::Block&    block,
    std::uint64_t         round,
    std::int64_t          now,
    const crypto::Signer& signer,
    p2p::GossipMesh&      gossip
) {
    try {
        ValidatorVoteRecord precommit = ValidatorVoteBuilder::buildPrecommit(
            validatorSetForBlock(runtime, block),
            block,
            round,
            now,
            signer
        );

        return submitAndBroadcastSignedVote(runtime, precommit, now, gossip);
    } catch (const std::exception& e) {
        return VoteCastResult::failed(
            std::string("Exception during PRECOMMIT: ") + e.what()
        );
    }
}

} // namespace nodo::consensus
