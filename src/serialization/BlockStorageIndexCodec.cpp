#include "serialization/BlockStorageIndexCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::serialization {

storage::BlockIndexEntry BlockStorageIndexCodec::deserializeEntry(
    const std::string& serialized
) {
    if (serialized.rfind("BlockIndexEntry{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a BlockIndexEntry.");
    }

    const std::uint64_t blockIndex =
        parseUnsigned64(
            FieldCodec::extractField(serialized, "blockIndex"),
            "blockIndex"
        );

    const std::string blockHash =
        FieldCodec::extractField(serialized, "blockHash");

    const std::string fileName =
        FieldCodec::extractField(serialized, "fileName");

    /*
     * These are development-format checks. The object then verifies the file
     * name matches block_<height>_<hash>.nodo exactly.
     */
    assertSafeHashLikeField(blockHash, "blockHash");
    assertSafeFileName(fileName, "fileName");

    storage::BlockIndexEntry entry(
        blockIndex,
        blockHash,
        fileName
    );

    if (!entry.isValid()) {
        throw std::invalid_argument("Deserialized BlockIndexEntry is invalid.");
    }

    if (entry.serialize() != serialized) {
        throw std::logic_error("BlockIndexEntry round-trip serialization mismatch.");
    }

    return entry;
}

std::vector<storage::BlockIndexEntry> BlockStorageIndexCodec::deserializeEntryList(
    const std::string& serializedList
) {
    std::vector<storage::BlockIndexEntry> entries;

    for (const auto& serializedEntry :
         FieldCodec::splitTopLevelObjects(serializedList, "BlockIndexEntry{")) {
        entries.push_back(
            deserializeEntry(serializedEntry)
        );
    }

    return entries;
}

storage::BlockStorageIndex BlockStorageIndexCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("BlockStorageIndex{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a BlockStorageIndex.");
    }

    const std::string indexVersion =
        FieldCodec::extractField(serialized, "indexVersion");

    const std::string chainManifestHash =
        FieldCodec::extractField(serialized, "chainManifestHash");

    const std::size_t blockCount =
        parseSize(
            FieldCodec::extractField(serialized, "blockCount"),
            "blockCount"
        );

    const std::int64_t createdAt =
        parseSigned64(
            FieldCodec::extractField(serialized, "createdAt"),
            "createdAt"
        );

    const std::string indexHash =
        FieldCodec::extractField(serialized, "indexHash");

    const std::string entriesList = FieldCodec::extractTrailingSection(
        serialized,
        ";entries=[",
        "]}"
    );

    std::vector<storage::BlockIndexEntry> entries =
        deserializeEntryList(entriesList);

    /*
     * These checks reject malformed metadata before the index object rebuilds
     * and verifies the full deterministic index hash.
     */
    assertSafeHashLikeField(chainManifestHash, "chainManifestHash");
    assertSafeHashLikeField(indexHash, "indexHash");

    storage::BlockStorageIndex index(
        indexVersion,
        chainManifestHash,
        blockCount,
        std::move(entries),
        createdAt,
        indexHash
    );

    if (!index.isValid()) {
        throw std::invalid_argument("Deserialized BlockStorageIndex is invalid.");
    }

    if (index.serialize() != serialized) {
        throw std::logic_error("BlockStorageIndex round-trip serialization mismatch.");
    }

    return index;
}

std::uint64_t BlockStorageIndexCodec::parseUnsigned64(
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

std::size_t BlockStorageIndexCodec::parseSize(
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

std::int64_t BlockStorageIndexCodec::parseSigned64(
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

void BlockStorageIndexCodec::assertSafeHashLikeField(
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

void BlockStorageIndexCodec::assertSafeFileName(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty file name field: " + fieldName);
    }

    if (value.find('/') != std::string::npos ||
        value.find('\\') != std::string::npos ||
        value.find("..") != std::string::npos) {
        throw std::invalid_argument("Unsafe file name field: " + fieldName);
    }

    for (const char current : value) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLower = current >= 'a' && current <= 'z';
        const bool isUpper = current >= 'A' && current <= 'Z';
        const bool isAllowedSymbol =
            current == '_' ||
            current == '-' ||
            current == '.';

        if (!isDigit && !isLower && !isUpper && !isAllowedSymbol) {
            throw std::invalid_argument("Unsafe file name field: " + fieldName);
        }
    }
}

} // namespace nodo::serialization