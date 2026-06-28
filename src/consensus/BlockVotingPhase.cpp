#include "consensus/BlockVotingPhase.hpp"

#include "consensus/ValidatorVoteBuilder.hpp"
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

        const VoteCollectResult collected = runtime.submitConsensusVote(prevote);
        if (!collected.accepted()) {
            return VoteCastResult::failed(
                "PREVOTE rejected by VotePool: " + collected.reason()
            );
        }

        const std::string serialized = prevote.serialize();
        gossip.broadcast(p2p::NetworkMessageType::VOTE_ANNOUNCE,  serialized, now);
        gossip.broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE, serialized, now);

        return VoteCastResult::ok();
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

        const VoteCollectResult collected = runtime.submitConsensusVote(precommit);
        if (!collected.accepted()) {
            return VoteCastResult::failed(
                "PRECOMMIT rejected by VotePool: " + collected.reason()
            );
        }

        const std::string serialized = precommit.serialize();
        gossip.broadcast(p2p::NetworkMessageType::VOTE_ANNOUNCE,  serialized, now);
        gossip.broadcast(p2p::NetworkMessageType::VALIDATOR_VOTE, serialized, now);

        return VoteCastResult::ok();
    } catch (const std::exception& e) {
        return VoteCastResult::failed(
            std::string("Exception during PRECOMMIT: ") + e.what()
        );
    }
}

} // namespace nodo::consensus
