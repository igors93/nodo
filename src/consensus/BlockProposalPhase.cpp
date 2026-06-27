#include "consensus/BlockProposalPhase.hpp"

#include "node/SignedBlockProposalMessage.hpp"
#include "p2p/NetworkEnvelope.hpp"

namespace nodo::consensus {

BlockProposalResult BlockProposalPhase::propose(
    const core::Block&               block,
    const std::string&               proposerAddress,
    std::uint64_t                    round,
    std::int64_t                     now,
    const crypto::Signer&            signer,
    p2p::GossipMesh&                 gossip,
    const crypto::SignatureProvider& provider
) {
    if (!block.isValid(false)) {
        return BlockProposalResult::skipped("Block is structurally invalid.");
    }

    try {
        const node::SignedBlockProposalMessage proposal =
            node::SignedBlockProposalMessage::sign(
                block,
                proposerAddress,
                signer.keyPair().publicKey(),
                signer.keyPair().privateKeyForSigningOnly(),
                round,
                now,
                provider
            );

        if (!proposal.isValid()) {
            return BlockProposalResult::skipped("Signed proposal failed validation.");
        }

        gossip.broadcast(
            p2p::NetworkMessageType::BLOCK_PROPOSAL,
            proposal.serialize(),
            now
        );

        return BlockProposalResult::ok();
    } catch (...) {
        return BlockProposalResult::skipped("Exception during proposal signing or broadcast.");
    }
}

} // namespace nodo::consensus
