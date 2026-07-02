#ifndef NODO_CONSENSUS_BLOCK_PROPOSAL_PHASE_HPP
#define NODO_CONSENSUS_BLOCK_PROPOSAL_PHASE_HPP

#include "core/Block.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/Signer.hpp"
#include "p2p/GossipMesh.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace nodo::consensus {

struct BlockProposalResult {
  bool proposed() const { return m_proposed; }
  const std::string &reason() const { return m_reason; }
  const std::string &serializedProposal() const { return m_serializedProposal; }

  static BlockProposalResult ok(std::string serializedProposal = "") {
    BlockProposalResult r;
    r.m_proposed = true;
    r.m_serializedProposal = std::move(serializedProposal);
    return r;
  }

  static BlockProposalResult skipped(std::string reason) {
    BlockProposalResult r;
    r.m_proposed = false;
    r.m_reason = std::move(reason);
    return r;
  }

private:
  bool m_proposed = false;
  std::string m_reason;
  std::string m_serializedProposal;
};

/*
 * BlockProposalPhase — Phase 2 of BFT consensus.
 *
 * Signs a validated candidate block as a formal proposal and broadcasts it
 * to peers via the gossip mesh so they can validate and cast their votes.
 *
 * This phase has no side effects on local chain state.
 */
class BlockProposalPhase {
public:
  /*
   * Sign the block as a proposal for (round, now) and broadcast via gossip.
   * The caller must ensure this node is the designated proposer for the
   * current height+round before calling.
   *
   * Returns ok() on successful broadcast; skipped() with a reason otherwise.
   */
  static BlockProposalResult
  propose(const core::Block &block, const std::string &proposerAddress,
          std::uint64_t round, std::int64_t now, const crypto::Signer &signer,
          p2p::GossipMesh &gossip, const crypto::SignatureProvider &provider);
};

} // namespace nodo::consensus

#endif
