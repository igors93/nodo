#include "node/BlockAnnounceHandler.hpp"

#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
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
    p2p::GossipMesh&                           gossip,
    core::Blockchain&                          blockchain,
    const core::StateTransitionPreviewContext& validationContext,
    std::int64_t                               /*now*/
) {
    std::vector<BlockAnnounceResult> results;

    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::BLOCK_ANNOUNCE
    );

    for (const auto& envelope : messages) {
        results.push_back(processEnvelope(envelope, blockchain, validationContext));
    }

    return results;
}

// ---------------------------------------------------------------------------
// processEnvelope
// ---------------------------------------------------------------------------

BlockAnnounceResult BlockAnnounceHandler::processEnvelope(
    const p2p::NetworkEnvelope&                envelope,
    core::Blockchain&                          blockchain,
    const core::StateTransitionPreviewContext& validationContext
) {
    // Decode block from payload.
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

    // Duplicate check.
    if (blockAlreadyKnown(blockchain, block)) {
        return BlockAnnounceResult{
            BlockAnnounceStatus::ALREADY_KNOWN,
            block.index(),
            block.hash(),
            "Block at height " + std::to_string(block.index()) + " already in local chain."
        };
    }

    // Full protocol commitment validation.
    // This checks canonical root format, parent linkage, and most importantly
    // recomputes stateRoot and receiptsRoot from local account state, then
    // compares them to the block's declared commitments.  A block is only
    // accepted if every commitment matches the local execution result.
    const core::BlockValidationResult validation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            validationContext
            // defaults to BlockValidationMode::ProtocolCommitment
        );

    if (!validation.accepted()) {
        const BlockAnnounceStatus announceStatus =
            (validation.status() == core::BlockValidationStatus::INVALID_PREVIOUS_HASH)
            ? BlockAnnounceStatus::CANNOT_APPEND
            : BlockAnnounceStatus::INVALID_BLOCK;

        return BlockAnnounceResult{
            announceStatus,
            block.index(),
            block.hash(),
            "Received block failed protocol commitment validation: " + validation.reason()
        };
    }

    // Append (only reached when all commitments verified).
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
