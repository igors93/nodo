#ifndef NODO_NODE_BLOCK_ANNOUNCE_HANDLER_HPP
#define NODO_NODE_BLOCK_ANNOUNCE_HANDLER_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class BlockAnnounceStatus {
    APPLIED,
    ALREADY_KNOWN,
    INVALID_BLOCK,
    CANNOT_APPEND,
    DECODE_FAILED
};

std::string blockAnnounceStatusToString(BlockAnnounceStatus status);

struct BlockAnnounceResult {
    BlockAnnounceStatus status;
    std::uint64_t      appliedHeight = 0;
    std::string        blockHash;
    std::string        reason;

    bool applied() const { return status == BlockAnnounceStatus::APPLIED; }
};

/*
 * BlockAnnounceHandler processes incoming BLOCK_ANNOUNCE messages from peers.
 *
 * Protocol flow:
 *  1. Decode Block from envelope payload (using Block::deserialize).
 *  2. Check if already in local blockchain (duplicate check by hash).
 *  3. Full protocol commitment validation:
 *     - Structural integrity (block.isValid with canonical roots)
 *     - Parent-hash linkage (canAppendBlock)
 *     - State transition: recompute stateRoot and receiptsRoot from local state
 *       and compare to block's declared commitments.
 *  4. Append to blockchain ONLY after validation passes.
 *
 * Security: blocks received from the network are NEVER accepted on structural
 * checks alone.  The caller must supply an authoritative
 * StateTransitionPreviewContext built from the local chain state, with
 * account-state enforcement, chain-bound crypto context and the canonical
 * protocol-domain executor.  If execution yields a different stateRoot or
 * receiptsRoot than what the block declares, the block is rejected.
 */
class BlockAnnounceHandler {
public:
    /*
     * Process all BLOCK_ANNOUNCE messages currently in the gossip inbox.
     * Drains the inbox for that type.  Returns results for each message.
     *
     * validationContext must be an authoritative protocol context built from
     * the current local chain state so computed stateRoot/receiptsRoot can be
     * compared against each announced block's declared commitments.
     */
    static std::vector<BlockAnnounceResult> processInbox(
        p2p::GossipMesh&                           gossip,
        core::Blockchain&                          blockchain,
        const core::StateTransitionPreviewContext& validationContext,
        std::int64_t                               now
    );

    /*
     * Process a single BLOCK_ANNOUNCE envelope.
     *
     * Runs full ProtocolCommitment validation against validationContext before
     * adding the block to the chain.
     */
    static BlockAnnounceResult processEnvelope(
        const p2p::NetworkEnvelope&                envelope,
        core::Blockchain&                          blockchain,
        const core::StateTransitionPreviewContext& validationContext
    );

    /*
     * Build a BLOCK_ANNOUNCE envelope from a local finalized block.
     * Call this after finalizing a block to broadcast it to peers.
     */
    static p2p::NetworkEnvelope buildAnnounceEnvelope(
        const core::Block&        block,
        const p2p::GossipMesh&    gossip,
        std::int64_t              now
    );

    /*
     * Broadcast a locally produced block to all connected peers.
     */
    static p2p::GossipDeliveryReport broadcastBlock(
        const core::Block& block,
        p2p::GossipMesh&   gossip,
        std::int64_t       now
    );
};

} // namespace nodo::node

#endif
