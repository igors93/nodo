#include "node/BlockSyncHandler.hpp"

#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "crypto/Hex.hpp"
#include "node/ChainSyncMessages.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <algorithm>
#include <limits>
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
static const std::string kCanonicalPayloadPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

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

std::optional<std::uint64_t> parseUint64(const std::string& value) {
    if (value.empty()) return std::nullopt;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
    }
    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size() ||
            parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

struct ParsedSyncRequest {
    bool        valid       = false;
    std::string requesterId;
    std::uint64_t fromHeight = 0;
    std::uint64_t maxBlocks  = 0;
};

std::string wrapCanonicalPayload(
    const std::vector<unsigned char>& bytes
) {
    return kCanonicalPayloadPrefix + crypto::hexEncode(bytes);
}

std::optional<std::vector<unsigned char>> unwrapCanonicalPayload(
    const std::string& payload
) {
    if (payload.rfind(kCanonicalPayloadPrefix, 0) != 0) {
        return std::nullopt;
    }

    const std::string hex =
        payload.substr(kCanonicalPayloadPrefix.size());
    if (!crypto::isHexString(hex)) {
        return std::nullopt;
    }

    try {
        return crypto::hexDecode(hex);
    } catch (...) {
        return std::nullopt;
    }
}

ParsedSyncRequest parseCanonicalSyncRequest(const std::string& payload) {
    const std::optional<std::vector<unsigned char>> bytes =
        unwrapCanonicalPayload(payload);
    if (!bytes.has_value()) {
        return {};
    }

    try {
        const NetworkBlockSyncRequest request =
            serialization::ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
                bytes.value()
            );
        if (!request.isValid()) {
            return {};
        }

        ParsedSyncRequest parsed;
        parsed.valid = true;
        parsed.requesterId = request.requesterNodeId();
        parsed.fromHeight = request.locator().fromHeight();
        parsed.maxBlocks = request.locator().maxBlocks();
        return parsed;
    } catch (...) {
        return {};
    }
}

ParsedSyncRequest parseLegacySyncRequest(const std::string& payload) {
    // Quick structural check
    if (payload.rfind("NetworkBlockSyncRequest{", 0) != 0) {
        return {};
    }

    ParsedSyncRequest out;
    out.requesterId = extractField(payload, "requesterNodeId");

    const std::string fromHeightStr = extractField(payload, "fromHeight");
    const std::string maxBlocksStr  = extractField(payload, "maxBlocks");
    const std::optional<std::uint64_t> fromHeight = parseUint64(fromHeightStr);
    const std::optional<std::uint64_t> maxBlocks = parseUint64(maxBlocksStr);

    if (!fromHeight.has_value() || !maxBlocks.has_value()) {
        return {};
    }

    out.fromHeight = fromHeight.value();
    out.maxBlocks  = maxBlocks.value();
    out.valid      = !out.requesterId.empty() && out.maxBlocks > 0;

    return out;
}

ParsedSyncRequest parseSyncRequest(const std::string& payload) {
    const ParsedSyncRequest canonical = parseCanonicalSyncRequest(payload);
    if (canonical.valid) {
        return canonical;
    }

    return parseLegacySyncRequest(payload);
}

std::optional<core::Block> tryDeserializeBlock(const std::string& payload) {
    try {
        return serialization::BlockCodec::deserialize(payload);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// serializeBlockList
// ---------------------------------------------------------------------------

std::string BlockSyncHandler::serializeBlockList(
    const std::vector<core::Block>& blocks
) {
    try {
        return wrapCanonicalPayload(
            serialization::ProtocolMessageCodec::encodeBlockList(blocks)
        );
    } catch (...) {
        return "";
    }
}

// ---------------------------------------------------------------------------
// deserializeBlockList
// ---------------------------------------------------------------------------

std::vector<core::Block> BlockSyncHandler::deserializeBlockList(
    const std::string& payload
) {
    std::vector<core::Block> result;

    const std::optional<std::vector<unsigned char>> canonicalBytes =
        unwrapCanonicalPayload(payload);
    if (canonicalBytes.has_value()) {
        try {
            return serialization::ProtocolMessageCodec::decodeBlockList(
                canonicalBytes.value()
            );
        } catch (...) {
            return {};
        }
    }

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
        const std::optional<std::uint64_t> parsedCount =
            parseUint64(countLine.substr(kCountPrefix.size()));
        if (!parsedCount.has_value()) {
            return result;
        }
        expectedCount = parsedCount.value();
    }
    if (expectedCount == 0) return result;
    if (expectedCount > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max()
        )) {
        return result;
    }

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

    if (chunks.size() != static_cast<std::size_t>(expectedCount)) {
        return {};
    }

    result.reserve(chunks.size());
    for (const auto& chunk : chunks) {
        if (chunk.empty()) return {};
        auto block = tryDeserializeBlock(chunk);
        if (block.has_value()) {
            result.push_back(std::move(block.value()));
        } else {
            return {};
        }
    }

    if (result.size() != static_cast<std::size_t>(expectedCount)) {
        return {};
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
    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::BLOCK_REQUEST
    );

    for (const auto& envelope : messages) {
        const ParsedSyncRequest req = parseSyncRequest(envelope.payload());
        if (!req.valid) {
            // Malformed request — skip silently.
            continue;
        }

        if (req.requesterId != envelope.senderNodeId()) {
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
        wrapCanonicalPayload(
            serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(
                request
            )
        ),
        now
    );
}

// ---------------------------------------------------------------------------
// applyResponses
// ---------------------------------------------------------------------------

std::size_t BlockSyncHandler::applyResponses(
    p2p::GossipMesh&  gossip,
    core::Blockchain& blockchain,
    std::function<core::StateTransitionPreviewContext(const core::Blockchain&)> contextBuilder,
    std::int64_t      /*now*/
) {
    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::BLOCK_RESPONSE
    );

    std::size_t applied = 0;

    for (const auto& envelope : messages) {
        const std::vector<core::Block> blocks =
            deserializeBlockList(envelope.payload());

        // Apply in order; stop at first block that fails full protocol
        // commitment validation.  The context is rebuilt from the current
        // chain state before each block so that the computed stateRoot and
        // receiptsRoot reflect all previously applied blocks.
        for (const auto& block : blocks) {
            const core::StateTransitionPreviewContext context =
                contextBuilder(blockchain);

            const core::BlockValidationResult validation =
                core::BlockStateTransitionValidator::validateCandidateBlock(
                    blockchain,
                    block,
                    context
                    // defaults to BlockValidationMode::ProtocolCommitment
                );

            if (!validation.accepted()) {
                break;
            }

            blockchain.addBlock(block);
            ++applied;
        }
    }

    return applied;
}

} // namespace nodo::node
