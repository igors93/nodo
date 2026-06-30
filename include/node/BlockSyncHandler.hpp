#ifndef NODO_NODE_BLOCK_SYNC_HANDLER_HPP
#define NODO_NODE_BLOCK_SYNC_HANDLER_HPP

#include "core/ProtocolLimits.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "node/ChainSyncMessages.hpp"
#include "p2p/GossipMesh.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nodo::node {

enum class BlockSyncQcMode {
    STATE_ROOT_ONLY,
    QC_REQUIRED
};

/*
 * BlockSyncHandler responds to BLOCK_REQUEST messages and applies BLOCK_RESPONSE.
 *
 * Protocol:
 *  Requester: sends BLOCK_REQUEST with BlockLocator{fromHeight, maxBlocks}
 *  Responder: reads its blockchain, serializes blocks fromHeight..min(fromHeight+maxBlocks, tip)
 *             and sends back a BLOCK_RESPONSE
 *  Requester: applies blocks in order to its local blockchain
 */
class BlockSyncHandler {
public:
    static constexpr std::size_t MAX_BLOCKS_PER_RESPONSE =
        core::ProtocolLimits::MAX_PERSISTENT_SYNC_BLOCK_BATCH;

    /*
     * Called on the SERVING node: read BLOCK_REQUEST messages from inbox,
     * respond with BLOCK_RESPONSE containing serialized blocks.
     */
    static void serveRequests(
        p2p::GossipMesh&        gossip,
        const core::Blockchain& blockchain,
        std::int64_t            now
    );

    /*
     * Called on the REQUESTING node: build and send a BLOCK_REQUEST for blocks
     * starting at fromHeight.
     */
    static p2p::GossipDeliveryReport requestBlocks(
        p2p::GossipMesh&   gossip,
        const std::string& localNodeId,
        std::uint64_t      fromHeight,
        std::uint64_t      maxBlocks,
        std::int64_t       now
    );

    /*
     * Called on the REQUESTING node: process BLOCK_RESPONSE messages,
     * apply received blocks to local blockchain in order.
     * Returns number of blocks applied.
     *
     * contextBuilder is called once per block with the current blockchain state
     * and must return an authoritative protocol context for the next block.
     * When qcMode is QC_REQUIRED, each block must be recorded as finalized in
     * finalizationRegistry before it is accepted. This protects against accepting
     * blocks that were never ratified by a supermajority of validators.
     *
     * If any block fails validation (or QC check) the entire remaining batch is
     * discarded; the function returns the count of blocks successfully applied.
     */
    static std::size_t applyResponses(
        p2p::GossipMesh&  gossip,
        core::Blockchain& blockchain,
        std::function<core::StateTransitionPreviewContext(const core::Blockchain&)> contextBuilder,
        const consensus::BlockFinalizationRegistry& finalizationRegistry,
        BlockSyncQcMode   qcMode,
        std::int64_t      now
    );

    // Serialize a list of blocks into a response payload.
    static std::string serializeBlockList(
        const std::vector<core::Block>& blocks
    );

    // Deserialize a response payload into a list of blocks.
    // Returns empty vector on failure.
    static std::vector<core::Block> deserializeBlockList(
        const std::string& payload
    );
};

} // namespace nodo::node

#endif
