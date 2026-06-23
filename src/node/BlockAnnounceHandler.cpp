#include "node/BlockAnnounceHandler.hpp"

#include "core/Block.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/BlockCodec.hpp"

#include <optional>
#include <sstream>

namespace nodo::node {

namespace {

std::optional<core::Block> decodeBlock(const std::string& payload) {
    try {
        return serialization::BlockCodec::deserialize(payload);
    } catch (...) {
        return std::nullopt;
    }
}

bool blockAlreadyKnown(
    const core::Blockchain& blockchain,
    const core::Block&      block
) {
    for (const auto& existing : blockchain.blocks()) {
        if (existing.index() == block.index() &&
            existing.hash()  == block.hash()) {
            return true;
        }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// blockAnnounceStatusToString
// ---------------------------------------------------------------------------

std::string blockAnnounceStatusToString(BlockAnnounceStatus status) {
    switch (status) {
        case BlockAnnounceStatus::APPLIED:       return "APPLIED";
        case BlockAnnounceStatus::ALREADY_KNOWN: return "ALREADY_KNOWN";
        case BlockAnnounceStatus::INVALID_BLOCK: return "INVALID_BLOCK";
        case BlockAnnounceStatus::CANNOT_APPEND: return "CANNOT_APPEND";
        case BlockAnnounceStatus::DECODE_FAILED: return "DECODE_FAILED";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// processInbox
// ---------------------------------------------------------------------------

std::vector<BlockAnnounceResult> BlockAnnounceHandler::processInbox(
    p2p::GossipMesh&  gossip,
    core::Blockchain& blockchain,
    std::int64_t      /*now*/
) {
    std::vector<BlockAnnounceResult> results;

    const auto messages = gossip.inbox().messagesForType(
        p2p::NetworkMessageType::BLOCK_ANNOUNCE
    );

    for (const auto& envelope : messages) {
        results.push_back(processEnvelope(envelope, blockchain));
    }

    return results;
}

// ---------------------------------------------------------------------------
// processEnvelope
// ---------------------------------------------------------------------------

BlockAnnounceResult BlockAnnounceHandler::processEnvelope(
    const p2p::NetworkEnvelope& envelope,
    core::Blockchain&           blockchain
) {
    // Step 1: decode block from payload.
    auto maybeBlock = decodeBlock(envelope.payload());
    if (!maybeBlock.has_value()) {
        return BlockAnnounceResult{
            BlockAnnounceStatus::DECODE_FAILED,
            0,
            "",
            "BLOCK_ANNOUNCE payload failed strict block decoding."
        };
    }

    const core::Block& block = maybeBlock.value();

    // Step 2: duplicate check.
    if (blockAlreadyKnown(blockchain, block)) {
        return BlockAnnounceResult{
            BlockAnnounceStatus::ALREADY_KNOWN,
            block.index(),
            block.hash(),
            "Block at height " + std::to_string(block.index()) + " already in local chain."
        };
    }

    // Step 3: structural validation.
    if (!block.isValid()) {
        return BlockAnnounceResult{
            BlockAnnounceStatus::INVALID_BLOCK,
            block.index(),
            block.hash(),
            "Received block failed structural validation."
        };
    }

    // Step 4: chain-append check.
    if (!blockchain.canAppendBlock(block)) {
        return BlockAnnounceResult{
            BlockAnnounceStatus::CANNOT_APPEND,
            block.index(),
            block.hash(),
            "Block cannot be appended to the current chain tip."
        };
    }

    // Step 5: append.
    blockchain.addBlock(block);

    return BlockAnnounceResult{
        BlockAnnounceStatus::APPLIED,
        block.index(),
        block.hash(),
        "Block applied at height " + std::to_string(block.index()) + "."
    };
}

// ---------------------------------------------------------------------------
// buildAnnounceEnvelope
// ---------------------------------------------------------------------------

p2p::NetworkEnvelope BlockAnnounceHandler::buildAnnounceEnvelope(
    const core::Block&     block,
    const p2p::GossipMesh& gossip,
    std::int64_t           now
) {
    return gossip.createEnvelope(
        p2p::NetworkMessageType::BLOCK_ANNOUNCE,
        block.serialize(),
        now
    );
}

// ---------------------------------------------------------------------------
// broadcastBlock
// ---------------------------------------------------------------------------

p2p::GossipDeliveryReport BlockAnnounceHandler::broadcastBlock(
    const core::Block& block,
    p2p::GossipMesh&   gossip,
    std::int64_t       now
) {
    return gossip.broadcast(
        p2p::NetworkMessageType::BLOCK_ANNOUNCE,
        block.serialize(),
        now
    );
}

} // namespace nodo::node
