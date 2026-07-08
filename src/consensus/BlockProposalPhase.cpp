#include "consensus/BlockProposalPhase.hpp"

#include "node/SignedBlockProposalMessage.hpp"
#include "p2p/NetworkEnvelope.hpp"

namespace nodo::consensus {

BlockProposalResult BlockProposalPhase::propose(
    const core::Block &block, const std::string &proposerAddress,
    std::uint64_t round, std::int64_t now, const crypto::Signer &signer,
    p2p::GossipMesh &gossip, const crypto::SignatureProvider &provider,
    const std::string &justification) {
  if (!block.isValid(false)) {
    return BlockProposalResult::skipped("Block is structurally invalid.");
  }

  try {
    const std::string payload =
        node::SignedBlockProposalMessage::buildSigningPayload(
            proposerAddress, signer.keyPair().publicKey(), block.hash(),
            block.index(), round, now, justification);
    const crypto::SignatureBundle bundle = signer.signValidatorPayload(
        payload, block.index(), round, block.hash(), now,
        crypto::SigningDomain::VALIDATOR_BLOCK_PROPOSAL);
    const node::SignedBlockProposalMessage proposal =
        node::SignedBlockProposalMessage::fromSignatureBundle(
            block, proposerAddress, signer.keyPair().publicKey(), round, now,
            bundle, justification);

    if (!proposal.isValid()) {
      return BlockProposalResult::skipped("Signed proposal failed validation.");
    }

    gossip.broadcast(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                     proposal.serialize(), now);

    return BlockProposalResult::ok(proposal.serialize());
  } catch (...) {
    return BlockProposalResult::skipped(
        "Exception during proposal signing or broadcast.");
  }
}

} // namespace nodo::consensus
