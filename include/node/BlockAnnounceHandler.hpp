#ifndef NODO_NODE_BLOCK_ANNOUNCE_HANDLER_HPP
#define NODO_NODE_BLOCK_ANNOUNCE_HANDLER_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
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
 * Flow:
 *  1. Decode Block from envelope payload (using Block::deserialize).
 *  2. Check if already in local blockchain (duplicate check by hash).
 *  3. Validate structural integrity (block.isValid()).
 *  4. Check blockchain.canAppendBlock().
 *  5. Append to blockchain.
 *  6. Optionally re-broadcast to other peers (gossip relay).
 */
class BlockAnnounceHandler {
public:
    /*
     * Process all BLOCK_ANNOUNCE messages currently in the gossip inbox.
     * Drains the inbox for that type. Returns results for each processed message.
     */
    static std::vector<BlockAnnounceResult> processInbox(
        p2p::GossipMesh&   gossip,
        core::Blockchain&  blockchain,
        std::int64_t       now
    );

    /*
     * Process a single BLOCK_ANNOUNCE envelope.
     */
    static BlockAnnounceResult processEnvelope(
        const p2p::NetworkEnvelope& envelope,
        core::Blockchain&           blockchain
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
