#include "serialization/ProtocolMessageCodec.hpp"

#include "serialization/BlockCodec.hpp"
#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace nodo::serialization {

namespace {

constexpr const char* CODEC_VERSION = "NODO_CANONICAL_PROTOCOL_V1";
constexpr std::size_t MAX_PROTOCOL_FIELD_BYTES = 16 * 1024 * 1024;
constexpr std::uint32_t MAX_BLOCK_LIST_COUNT = 1024;

void writeHeader(
    CanonicalWriter& writer,
    const std::string& typeName
) {
    writer.writeString(CODEC_VERSION);
    writer.writeString(typeName);
}

void readHeader(
    CanonicalReader& reader,
    const std::string& expectedType
) {
    const std::string version = reader.readString();
    const std::string type = reader.readString();

    if (version != CODEC_VERSION) {
        throw std::runtime_error("Unsupported canonical protocol codec version.");
    }

    if (type != expectedType) {
        throw std::runtime_error("Unexpected canonical protocol message type.");
    }
}

std::uint32_t encodeMessageType(
    p2p::NetworkMessageType type
) {
    return static_cast<std::uint32_t>(type);
}

p2p::NetworkMessageType decodeMessageType(
    std::uint32_t value
) {
    if (value > static_cast<std::uint32_t>(
            p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE)) {
        return p2p::NetworkMessageType::UNKNOWN;
    }

    return static_cast<p2p::NetworkMessageType>(value);
}

} // namespace

std::vector<unsigned char> ProtocolMessageCodec::encodeNetworkEnvelope(
    const p2p::NetworkEnvelope& envelope
) {
    CanonicalWriter writer;
    writeHeader(writer, "NetworkEnvelope");
    writer.writeString(envelope.networkId());
    writer.writeString(envelope.chainId());
    writer.writeString(envelope.protocolVersion());
    writer.writeUInt32(encodeMessageType(envelope.messageType()));
    writer.writeString(envelope.senderNodeId());
    writer.writeInt64(envelope.createdAt());
    writer.writeUInt32(envelope.ttlSeconds());
    writer.writeString(envelope.payload());
    return writer.bytes();
}

p2p::NetworkEnvelope ProtocolMessageCodec::decodeNetworkEnvelope(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "NetworkEnvelope");

    const std::string networkId = reader.readString();
    const std::string chainId = reader.readString();
    const std::string protocolVersion = reader.readString();
    const p2p::NetworkMessageType messageType =
        decodeMessageType(reader.readUInt32());
    const std::string senderNodeId = reader.readString();
    const std::int64_t createdAt = reader.readInt64();
    const std::uint32_t ttlSeconds = reader.readUInt32();
    const std::string payload = reader.readString();

    p2p::NetworkEnvelope envelope(
        networkId,
        chainId,
        protocolVersion,
        messageType,
        senderNodeId,
        createdAt,
        ttlSeconds,
        payload
    );

    reader.requireFullyConsumed();
    return envelope;
}

std::string ProtocolMessageCodec::hashNetworkEnvelope(
    const p2p::NetworkEnvelope& envelope
) {
    return CanonicalHash::hashBytes(
        encodeNetworkEnvelope(envelope),
        "NODO_NETWORK_ENVELOPE_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ProtocolMessageCodec::encodeChainStatusMessage(
    const node::ChainStatusMessage& message
) {
    CanonicalWriter writer;
    writeHeader(writer, "ChainStatusMessage");
    writer.writeString(message.networkId());
    writer.writeString(message.chainId());
    writer.writeString(message.protocolVersion());
    writer.writeUInt64(message.latestHeight());
    writer.writeString(message.latestBlockHash());
    writer.writeUInt64(message.finalizedHeight());
    writer.writeString(message.finalizedBlockHash());
    return writer.bytes();
}

node::ChainStatusMessage ProtocolMessageCodec::decodeChainStatusMessage(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "ChainStatusMessage");

    const std::string networkId = reader.readString();
    const std::string chainId = reader.readString();
    const std::string protocolVersion = reader.readString();
    const std::uint64_t latestHeight = reader.readUInt64();
    const std::string latestBlockHash = reader.readString();
    const std::uint64_t finalizedHeight = reader.readUInt64();
    const std::string finalizedBlockHash = reader.readString();

    node::ChainStatusMessage message(
        networkId,
        chainId,
        protocolVersion,
        latestHeight,
        latestBlockHash,
        finalizedHeight,
        finalizedBlockHash
    );

    reader.requireFullyConsumed();
    return message;
}

std::string ProtocolMessageCodec::hashChainStatusMessage(
    const node::ChainStatusMessage& message
) {
    return CanonicalHash::hashBytes(
        encodeChainStatusMessage(message),
        "NODO_CHAIN_STATUS_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ProtocolMessageCodec::encodeBlockLocator(
    const node::BlockLocator& locator
) {
    CanonicalWriter writer;
    writeHeader(writer, "BlockLocator");
    writer.writeUInt64(locator.fromHeight());
    writer.writeUInt64(locator.maxBlocks());
    writer.writeUInt32(static_cast<std::uint32_t>(locator.knownAncestorHashes().size()));

    for (const auto& hash : locator.knownAncestorHashes()) {
        writer.writeString(hash);
    }

    return writer.bytes();
}

node::BlockLocator ProtocolMessageCodec::decodeBlockLocator(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "BlockLocator");

    const std::uint64_t fromHeight = reader.readUInt64();
    const std::uint64_t maxBlocks = reader.readUInt64();
    const std::uint32_t hashCount = reader.readUInt32();

    std::vector<std::string> hashes;
    hashes.reserve(hashCount);

    for (std::uint32_t index = 0; index < hashCount; ++index) {
        hashes.push_back(reader.readString());
    }

    reader.requireFullyConsumed();
    return node::BlockLocator(fromHeight, maxBlocks, hashes);
}

std::string ProtocolMessageCodec::hashBlockLocator(
    const node::BlockLocator& locator
) {
    return CanonicalHash::hashBytes(
        encodeBlockLocator(locator),
        "NODO_BLOCK_LOCATOR_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ProtocolMessageCodec::encodeNetworkBlockSyncRequest(
    const node::NetworkBlockSyncRequest& request
) {
    CanonicalWriter writer;
    writeHeader(writer, "NetworkBlockSyncRequest");
    writer.writeString(request.requesterNodeId());
    writer.writeBytes(encodeBlockLocator(request.locator()));
    writer.writeInt64(request.createdAt());
    return writer.bytes();
}

node::NetworkBlockSyncRequest ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "NetworkBlockSyncRequest");

    const std::string requesterNodeId = reader.readString();
    const auto locator = decodeBlockLocator(reader.readBytes());
    const std::int64_t createdAt = reader.readInt64();

    node::NetworkBlockSyncRequest request(
        requesterNodeId,
        locator,
        createdAt
    );

    reader.requireFullyConsumed();
    return request;
}

std::string ProtocolMessageCodec::hashNetworkBlockSyncRequest(
    const node::NetworkBlockSyncRequest& request
) {
    return CanonicalHash::hashBytes(
        encodeNetworkBlockSyncRequest(request),
        "NODO_NETWORK_BLOCK_SYNC_REQUEST_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ProtocolMessageCodec::encodeBlockList(
    const std::vector<core::Block>& blocks
) {
    if (blocks.size() > MAX_BLOCK_LIST_COUNT) {
        throw std::length_error("Canonical block list exceeds protocol limit.");
    }

    CanonicalWriter writer;
    writeHeader(writer, "BlockList");
    writer.writeUInt32(static_cast<std::uint32_t>(blocks.size()));

    for (const auto& block : blocks) {
        if (!block.isValid(false)) {
            throw std::invalid_argument("Invalid block rejected by canonical block list codec.");
        }
        writer.writeString(block.serialize());
    }

    return writer.bytes();
}

std::vector<core::Block> ProtocolMessageCodec::decodeBlockList(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "BlockList");

    const std::uint32_t blockCount = reader.readUInt32();
    if (blockCount > MAX_BLOCK_LIST_COUNT) {
        throw std::length_error("Canonical block list exceeds protocol limit.");
    }

    std::vector<core::Block> blocks;
    blocks.reserve(blockCount);

    for (std::uint32_t index = 0; index < blockCount; ++index) {
        const std::string serializedBlock = reader.readString();
        core::Block decoded = BlockCodec::deserialize(serializedBlock);
        if (!decoded.isValid(false)) {
            throw std::runtime_error("Canonical block list contains invalid block.");
        }
        blocks.push_back(std::move(decoded));
    }

    reader.requireFullyConsumed();
    return blocks;
}

std::string ProtocolMessageCodec::hashBlockList(
    const std::vector<core::Block>& blocks
) {
    return CanonicalHash::hashBytes(
        encodeBlockList(blocks),
        "NODO_BLOCK_LIST_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ProtocolMessageCodec::encodeRoundAdvanceMessage(
    const node::RoundAdvanceMessage& message
) {
    if (!message.isValid()) {
        throw std::invalid_argument("Invalid round advance message rejected by canonical codec.");
    }

    CanonicalWriter writer;
    writeHeader(writer, "RoundAdvanceMessage");
    writer.writeUInt64(message.height());
    writer.writeUInt64(message.round());
    writer.writeString(message.proposerAddress());
    writer.writeInt64(message.createdAt());
    return writer.bytes();
}

node::RoundAdvanceMessage ProtocolMessageCodec::decodeRoundAdvanceMessage(
    const std::vector<unsigned char>& bytes
) {
    CanonicalReader reader(bytes, MAX_PROTOCOL_FIELD_BYTES);
    readHeader(reader, "RoundAdvanceMessage");

    const std::uint64_t height = reader.readUInt64();
    const std::uint64_t round = reader.readUInt64();
    const std::string proposerAddress = reader.readString();
    const std::int64_t createdAt = reader.readInt64();

    node::RoundAdvanceMessage message(
        height,
        round,
        proposerAddress,
        createdAt
    );

    reader.requireFullyConsumed();
    if (!message.isValid()) {
        throw std::runtime_error("Canonical round advance message is invalid.");
    }

    return message;
}

std::string ProtocolMessageCodec::hashRoundAdvanceMessage(
    const node::RoundAdvanceMessage& message
) {
    return CanonicalHash::hashBytes(
        encodeRoundAdvanceMessage(message),
        "NODO_ROUND_ADVANCE_CANONICAL_HASH_V1"
    );
}

} // namespace nodo::serialization
