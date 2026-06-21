#include "node/BlockSyncHandler.hpp"

#include "core/Block.hpp"
#include "node/ChainSyncMessages.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace nodo::node {

namespace {

// Block list format:
//   NODO_BLOCK_LIST_V1\ncount=N\n<block0>\n---NODO_BLOCK_SEP---\n<block1>...
static const std::string kBlockListHeader     = "NODO_BLOCK_LIST_V1";
static const std::string kBlockSeparator      = "\n---NODO_BLOCK_SEP---\n";
static const std::string kCountPrefix         = "count=";

// ---------------------------------------------------------------------------
// NetworkBlockSyncRequest parsing
//
// TODO: NetworkBlockSyncRequest::deserialize(const std::string&) does not
// exist yet. Until it is added, we parse the serialized form produced by
// NetworkBlockSyncRequest::serialize() by hand.
//
// Serialized form (from ChainSyncMessages.cpp):
//   NetworkBlockSyncRequest{requesterNodeId=<id>;locator=BlockLocator{fromHeight=<h>;maxBlocks=<m>;knownAncestorHashes=[<h1>,...]};createdAt=<t>}
//
// We only extract fromHeight and maxBlocks for the serving logic.
// ---------------------------------------------------------------------------

std::string extractField(const std::string& text, const std::string& key) {
    const std::string prefix = key + "=";
    const auto pos = text.find(prefix);
    if (pos == std::string::npos) {
        return "";
    }
    const auto valueStart = pos + prefix.size();
    // Value ends at first ';', '{', '}', or '[' that is not inside braces.
    std::size_t depth = 0;
    std::size_t i = valueStart;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            if (depth == 0) break;
            --depth;
        } else if ((c == ';') && depth == 0) {
            break;
        }
        ++i;
    }
    return text.substr(valueStart, i - valueStart);
}

std::uint64_t parseUint64(const std::string& value, std::uint64_t fallback = 0) {
    if (value.empty()) return fallback;
    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (...) {
        return fallback;
    }
}

struct ParsedSyncRequest {
    bool        valid       = false;
    std::string requesterId;
    std::uint64_t fromHeight = 0;
    std::uint64_t maxBlocks  = 0;
};

ParsedSyncRequest parseSyncRequest(const std::string& payload) {
    // Quick structural check
    if (payload.rfind("NetworkBlockSyncRequest{", 0) != 0) {
        return {};
    }

    ParsedSyncRequest out;
    out.requesterId = extractField(payload, "requesterNodeId");

    const std::string fromHeightStr = extractField(payload, "fromHeight");
    const std::string maxBlocksStr  = extractField(payload, "maxBlocks");

    out.fromHeight = parseUint64(fromHeightStr);
    out.maxBlocks  = parseUint64(maxBlocksStr);
    out.valid      = !out.requesterId.empty() && out.maxBlocks > 0;

    return out;
}

std::optional<core::Block> tryDeserializeBlock(const std::string& payload) {
    return core::Block::deserialize(payload);
}

} // namespace

// ---------------------------------------------------------------------------
// serializeBlockList
// ---------------------------------------------------------------------------

std::string BlockSyncHandler::serializeBlockList(
    const std::vector<core::Block>& blocks
) {
    std::ostringstream oss;
    oss << kBlockListHeader << "\n"
        << kCountPrefix << blocks.size() << "\n";

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (i > 0) {
            oss << kBlockSeparator;
        }
        oss << blocks[i].serialize();
    }

    return oss.str();
}

// ---------------------------------------------------------------------------
// deserializeBlockList
// ---------------------------------------------------------------------------

std::vector<core::Block> BlockSyncHandler::deserializeBlockList(
    const std::string& payload
) {
    std::vector<core::Block> result;

    // Check header.
    if (payload.rfind(kBlockListHeader, 0) != 0) return result;

    // Find first newline (after header line).
    const auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos) return result;

    // Find second newline (after count= line).
    const auto secondNewline = payload.find('\n', firstNewline + 1);
    if (secondNewline == std::string::npos) return result;

    // Parse expected count.
    const std::string countLine = payload.substr(
        firstNewline + 1, secondNewline - firstNewline - 1
    );
    std::uint64_t expectedCount = 0;
    if (countLine.rfind(kCountPrefix, 0) == 0) {
        try {
            expectedCount = std::stoull(countLine.substr(kCountPrefix.size()));
        } catch (...) {}
    }
    if (expectedCount == 0) return result;

    // Split by block separator.
    const std::string blocksStr = payload.substr(secondNewline + 1);
    std::vector<std::string> chunks;
    std::size_t pos = 0;
    while (true) {
        const auto sepPos = blocksStr.find(kBlockSeparator, pos);
        if (sepPos == std::string::npos) {
            chunks.push_back(blocksStr.substr(pos));
            break;
        }
        chunks.push_back(blocksStr.substr(pos, sepPos - pos));
        pos = sepPos + kBlockSeparator.size();
    }

    result.reserve(chunks.size());
    for (const auto& chunk : chunks) {
        if (chunk.empty()) continue;
        auto block = tryDeserializeBlock(chunk);
        if (block.has_value()) {
            result.push_back(std::move(block.value()));
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// serveRequests
// ---------------------------------------------------------------------------

void BlockSyncHandler::serveRequests(
    p2p::GossipMesh&        gossip,
    const core::Blockchain& blockchain,
    std::int64_t            now
) {
    const auto messages = gossip.inbox().messagesForType(
        p2p::NetworkMessageType::BLOCK_REQUEST
    );

    for (const auto& envelope : messages) {
        const ParsedSyncRequest req = parseSyncRequest(envelope.payload());
        if (!req.valid) {
            // Malformed request — skip silently.
            continue;
        }

        const std::size_t chainSize = blockchain.size();
        if (chainSize == 0) {
            continue;
        }

        const std::uint64_t fromHeight  = req.fromHeight;
        const std::uint64_t maxToSend   = std::min(
            static_cast<std::uint64_t>(MAX_BLOCKS_PER_RESPONSE),
            req.maxBlocks
        );

        // Collect blocks [fromHeight, fromHeight + maxToSend) that exist.
        std::vector<core::Block> toSend;
        toSend.reserve(static_cast<std::size_t>(maxToSend));

        for (const auto& block : blockchain.blocks()) {
            if (block.index() < fromHeight) {
                continue;
            }
            if (toSend.size() >= static_cast<std::size_t>(maxToSend)) {
                break;
            }
            toSend.push_back(block);
        }

        if (toSend.empty()) {
            continue;
        }

        const std::string responsePayload = serializeBlockList(toSend);
        gossip.broadcast(
            p2p::NetworkMessageType::BLOCK_RESPONSE,
            responsePayload,
            now
        );
    }
}

// ---------------------------------------------------------------------------
// requestBlocks
// ---------------------------------------------------------------------------

p2p::GossipDeliveryReport BlockSyncHandler::requestBlocks(
    p2p::GossipMesh&   gossip,
    const std::string& localNodeId,
    std::uint64_t      fromHeight,
    std::uint64_t      maxBlocks,
    std::int64_t       now
) {
    // BlockLocator::isValid() requires knownAncestorHashes to be non-empty.
    // We use the fromHeight as a string anchor so the locator validates.
    const std::vector<std::string> ancestorHints = {
        "height-" + std::to_string(fromHeight)
    };

    const std::uint64_t clampedMax = (maxBlocks == 0)
        ? MAX_BLOCKS_PER_RESPONSE
        : std::min(maxBlocks, static_cast<std::uint64_t>(MAX_BLOCKS_PER_RESPONSE));

    const BlockLocator locator(fromHeight, clampedMax, ancestorHints);
    const NetworkBlockSyncRequest request(localNodeId, locator, now);

    return gossip.broadcast(
        p2p::NetworkMessageType::BLOCK_REQUEST,
        request.serialize(),
        now
    );
}

// ---------------------------------------------------------------------------
// applyResponses
// ---------------------------------------------------------------------------

std::size_t BlockSyncHandler::applyResponses(
    p2p::GossipMesh&  gossip,
    core::Blockchain& blockchain,
    std::int64_t      /*now*/
) {
    const auto messages = gossip.inbox().messagesForType(
        p2p::NetworkMessageType::BLOCK_RESPONSE
    );

    std::size_t applied = 0;

    for (const auto& envelope : messages) {
        const std::vector<core::Block> blocks =
            deserializeBlockList(envelope.payload());

        // Apply in order; stop at first block that cannot be appended.
        for (const auto& block : blocks) {
            if (!block.isValid()) {
                break;
            }
            if (!blockchain.canAppendBlock(block)) {
                break;
            }
            blockchain.addBlock(block);
            ++applied;
        }
    }

    return applied;
}

} // namespace nodo::node
