#include "serialization/BlockSnapshotHeaderCodec.hpp"

#include "crypto/hash.h"
#include "serialization/FieldCodec.hpp"

#include <limits>
#include <stdexcept>

namespace nodo::serialization {

storage::BlockSnapshotHeader BlockSnapshotHeaderCodec::deserializeFromSerializedBlock(
    const std::string& serializedBlock
) {
    if (serializedBlock.rfind("Block{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a Block.");
    }

    const std::uint64_t blockIndex =
        parseUnsigned64(
            FieldCodec::extractField(serializedBlock, "index"),
            "index"
        );

    const std::string previousHash =
        FieldCodec::extractField(serializedBlock, "previousHash");

    const std::string blockHash =
        FieldCodec::extractField(serializedBlock, "hash");

    const std::int64_t timestamp =
        parseSigned64(
            FieldCodec::extractField(serializedBlock, "timestamp"),
            "timestamp"
        );

    const std::size_t recordCount =
        parseSize(
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

    assertSafePreviousHash(previousHash, "previousHash");
    assertSafeHashLikeField(blockHash, "hash");

    storage::BlockSnapshotHeader header(
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

std::string BlockSnapshotHeaderCodec::extractHeaderPayload(
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

std::size_t BlockSnapshotHeaderCodec::countLedgerRecordsInHeaderPayload(
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

bool BlockSnapshotHeaderCodec::headerPayloadMatchesMetadata(
    const std::string& headerPayload,
    std::uint64_t blockIndex,
    const std::string& previousHash,
    std::int64_t timestamp,
    std::size_t recordCount
) {
    try {
        const std::uint64_t payloadIndex =
            parseUnsigned64(
                FieldCodec::extractField(headerPayload, "index"),
                "payload.index"
            );

        const std::string payloadPreviousHash =
            FieldCodec::extractField(headerPayload, "previousHash");

        const std::int64_t payloadTimestamp =
            parseSigned64(
                FieldCodec::extractField(headerPayload, "timestamp"),
                "payload.timestamp"
            );

        const std::size_t payloadRecordCount =
            countLedgerRecordsInHeaderPayload(headerPayload);

        if (payloadIndex != blockIndex) {
            return false;
        }

        if (payloadPreviousHash != previousHash) {
            return false;
        }

        if (payloadTimestamp != timestamp) {
            return false;
        }

        if (payloadRecordCount != recordCount) {
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::uint64_t BlockSnapshotHeaderCodec::parseUnsigned64(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        if (!value.empty() && value.front() == '-') {
            throw std::invalid_argument("Negative unsigned integer");
        }

        std::size_t parsedCharacters = 0;
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);

        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }

        if (parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            throw std::out_of_range("Unsigned integer is too large");
        }

        return static_cast<std::uint64_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid unsigned integer field: " + fieldName);
    }
}

std::size_t BlockSnapshotHeaderCodec::parseSize(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        if (!value.empty() && value.front() == '-') {
            throw std::invalid_argument("Negative size value");
        }

        std::size_t parsedCharacters = 0;
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);

        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }

        if (parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::size_t>::max()
            )) {
            throw std::out_of_range("Size value is too large");
        }

        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid size field: " + fieldName);
    }
}

std::int64_t BlockSnapshotHeaderCodec::parseSigned64(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        std::size_t parsedCharacters = 0;
        const std::int64_t parsed = std::stoll(value, &parsedCharacters);

        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }

        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid signed integer field: " + fieldName);
    }
}

void BlockSnapshotHeaderCodec::assertSafeHashLikeField(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty hash-like field: " + fieldName);
    }

    for (const char current : value) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';
        const bool isUpperHex = current >= 'A' && current <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            throw std::invalid_argument("Unsafe hash-like field: " + fieldName);
        }
    }
}

void BlockSnapshotHeaderCodec::assertSafePreviousHash(
    const std::string& value,
    const std::string& fieldName
) {
    if (value == "GENESIS") {
        return;
    }

    assertSafeHashLikeField(value, fieldName);
}

std::string BlockSnapshotHeaderCodec::hashString(
    const std::string& value
) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::serialization