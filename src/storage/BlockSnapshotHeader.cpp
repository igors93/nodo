#include "storage/BlockSnapshotHeader.hpp"

#include "crypto/hash.h"
#include "serialization/FieldCodec.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

using nodo::serialization::FieldCodec;

namespace {

std::uint64_t parseUnsigned64(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid unsigned integer field: " + fieldName);
    }
}

std::size_t parseSize(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        return static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid size field: " + fieldName);
    }
}

std::int64_t parseInt64(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        return std::stoll(value);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid signed integer field: " + fieldName);
    }
}

} // namespace

BlockSnapshotHeader BlockSnapshotHeader::fromSerializedBlock(
    const std::string& serializedBlock
) {
    if (serializedBlock.rfind("Block{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a Block.");
    }

    const std::uint64_t blockIndex = parseUnsigned64(
        FieldCodec::extractField(serializedBlock, "index"),
        "index"
    );

    const std::string previousHash =
        FieldCodec::extractField(serializedBlock, "previousHash");

    const std::string blockHash =
        FieldCodec::extractField(serializedBlock, "hash");

    const std::int64_t timestamp = parseInt64(
        FieldCodec::extractField(serializedBlock, "timestamp"),
        "timestamp"
    );

    const std::size_t recordCount = parseSize(
        FieldCodec::extractField(serializedBlock, "recordCount"),
        "recordCount"
    );

    const std::string headerPayload =
        extractHeaderPayload(serializedBlock);

    const std::size_t parsedRecordCount =
        countLedgerRecordsInHeaderPayload(headerPayload);

    if (recordCount != parsedRecordCount) {
        throw std::logic_error("Block snapshot record count does not match payload.");
    }

    BlockSnapshotHeader header(
        blockIndex,
        previousHash,
        blockHash,
        timestamp,
        recordCount,
        headerPayload,
        hashString(headerPayload)
    );

    if (!header.isValid()) {
        throw std::logic_error("Parsed BlockSnapshotHeader is invalid.");
    }

    return header;
}

BlockSnapshotHeader BlockSnapshotHeader::fromFile(
    const std::string& filePath
) {
    return fromSerializedBlock(readFile(filePath));
}

bool BlockSnapshotHeader::validateHeaderSequence(
    const std::vector<BlockSnapshotHeader>& headers
) {
    if (headers.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (!headers[i].isValid()) {
            return false;
        }

        if (headers[i].blockIndex() != i) {
            return false;
        }

        if (i == 0) {
            if (!headers[i].isGenesisHeader()) {
                return false;
            }

            continue;
        }

        if (headers[i].previousHash() != headers[i - 1].blockHash()) {
            return false;
        }
    }

    return true;
}

BlockSnapshotHeader::BlockSnapshotHeader(
    std::uint64_t blockIndex,
    std::string previousHash,
    std::string blockHash,
    std::int64_t timestamp,
    std::size_t recordCount,
    std::string headerPayload,
    std::string calculatedHash
)
    : m_blockIndex(blockIndex),
      m_previousHash(std::move(previousHash)),
      m_blockHash(std::move(blockHash)),
      m_timestamp(timestamp),
      m_recordCount(recordCount),
      m_headerPayload(std::move(headerPayload)),
      m_calculatedHash(std::move(calculatedHash)) {}

std::uint64_t BlockSnapshotHeader::blockIndex() const {
    return m_blockIndex;
}

const std::string& BlockSnapshotHeader::previousHash() const {
    return m_previousHash;
}

const std::string& BlockSnapshotHeader::blockHash() const {
    return m_blockHash;
}

std::int64_t BlockSnapshotHeader::timestamp() const {
    return m_timestamp;
}

std::size_t BlockSnapshotHeader::recordCount() const {
    return m_recordCount;
}

const std::string& BlockSnapshotHeader::headerPayload() const {
    return m_headerPayload;
}

const std::string& BlockSnapshotHeader::calculatedHash() const {
    return m_calculatedHash;
}

bool BlockSnapshotHeader::isGenesisHeader() const {
    return m_blockIndex == 0 && m_previousHash == "GENESIS";
}

bool BlockSnapshotHeader::isValid() const {
    if (!isSafePreviousHash(m_previousHash)) {
        return false;
    }

    if (!isSafeHash(m_blockHash)) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_recordCount == 0) {
        return false;
    }

    if (m_headerPayload.empty()) {
        return false;
    }

    if (m_headerPayload.rfind("BlockHeader{", 0) != 0) {
        return false;
    }

    if (!isSafeHash(m_calculatedHash)) {
        return false;
    }

    if (m_blockHash != m_calculatedHash) {
        return false;
    }

    if (m_blockIndex == 0 && m_previousHash != "GENESIS") {
        return false;
    }

    if (m_blockIndex > 0 && m_previousHash == "GENESIS") {
        return false;
    }

    try {
        const std::uint64_t payloadIndex = parseUnsigned64(
            FieldCodec::extractField(m_headerPayload, "index"),
            "payload.index"
        );

        const std::string payloadPreviousHash =
            FieldCodec::extractField(m_headerPayload, "previousHash");

        const std::int64_t payloadTimestamp = parseInt64(
            FieldCodec::extractField(m_headerPayload, "timestamp"),
            "payload.timestamp"
        );

        const std::size_t payloadRecordCount =
            countLedgerRecordsInHeaderPayload(m_headerPayload);

        if (payloadIndex != m_blockIndex) {
            return false;
        }

        if (payloadPreviousHash != m_previousHash) {
            return false;
        }

        if (payloadTimestamp != m_timestamp) {
            return false;
        }

        if (payloadRecordCount != m_recordCount) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return true;
}

std::string BlockSnapshotHeader::serialize() const {
    std::ostringstream oss;

    oss << "BlockSnapshotHeader{"
        << "blockIndex=" << m_blockIndex
        << ";previousHash=" << m_previousHash
        << ";blockHash=" << m_blockHash
        << ";timestamp=" << m_timestamp
        << ";recordCount=" << m_recordCount
        << ";calculatedHash=" << m_calculatedHash
        << "}";

    return oss.str();
}

std::string BlockSnapshotHeader::extractHeaderPayload(
    const std::string& serializedBlock
) {
    const std::string marker = ";payload=";
    const std::size_t markerPosition = serializedBlock.find(marker);

    if (markerPosition == std::string::npos) {
        throw std::invalid_argument("Serialized Block is missing payload.");
    }

    const std::size_t payloadStart = markerPosition + marker.size();

    if (serializedBlock.size() <= payloadStart + 1) {
        throw std::invalid_argument("Serialized Block payload is empty.");
    }

    if (serializedBlock.back() != '}') {
        throw std::invalid_argument("Serialized Block is missing closing brace.");
    }

    const std::string payload =
        serializedBlock.substr(payloadStart, serializedBlock.size() - payloadStart - 1);

    if (payload.rfind("BlockHeader{", 0) != 0) {
        throw std::invalid_argument("Serialized Block payload is not a BlockHeader.");
    }

    return payload;
}

std::size_t BlockSnapshotHeader::countLedgerRecordsInHeaderPayload(
    const std::string& headerPayload
) {
    if (headerPayload.rfind("BlockHeader{", 0) != 0) {
        throw std::invalid_argument("Serialized payload is not a BlockHeader.");
    }

    const std::string recordsList = FieldCodec::extractTrailingSection(
        headerPayload,
        ";records=[",
        "]}"
    );

    return FieldCodec::splitTopLevelObjects(
        recordsList,
        "LedgerRecord{"
    ).size();
}

std::string BlockSnapshotHeader::readFile(
    const std::string& filePath
) {
    if (filePath.empty()) {
        throw std::invalid_argument("Block snapshot file path cannot be empty.");
    }

    std::ifstream input(filePath, std::ios::in | std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Failed to open block snapshot file for header parsing.");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed while reading block snapshot for header parsing.");
    }

    return buffer.str();
}

bool BlockSnapshotHeader::isSafeHash(
    const std::string& hash
) {
    if (hash.empty()) {
        return false;
    }

    for (const char current : hash) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';
        const bool isUpperHex = current >= 'A' && current <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            return false;
        }
    }

    return true;
}

bool BlockSnapshotHeader::isSafePreviousHash(
    const std::string& previousHash
) {
    if (previousHash == "GENESIS") {
        return true;
    }

    return isSafeHash(previousHash);
}

std::string BlockSnapshotHeader::hashString(
    const std::string& value
) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::storage
