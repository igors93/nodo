#include "serialization/ChainManifestCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <limits>
#include <stdexcept>

namespace nodo::serialization {

storage::ChainManifest ChainManifestCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("ChainManifest{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a ChainManifest.");
    }

    const std::string chainVersion =
        FieldCodec::extractField(serialized, "chainVersion");

    const std::size_t blockCount =
        parseSize(
            FieldCodec::extractField(serialized, "blockCount"),
            "blockCount"
        );

    const std::string genesisHash =
        FieldCodec::extractField(serialized, "genesisHash");

    const std::string latestHash =
        FieldCodec::extractField(serialized, "latestHash");

    const std::int64_t createdAt =
        parseSigned64(
            FieldCodec::extractField(serialized, "createdAt"),
            "createdAt"
        );

    const std::string manifestHash =
        FieldCodec::extractField(serialized, "manifestHash");

    /*
     * These are development-format checks. They intentionally reject obvious
     * malformed metadata before the manifest object recalculates its own hash.
     */
    assertSafeHashLikeField(genesisHash, "genesisHash");
    assertSafeHashLikeField(latestHash, "latestHash");
    assertSafeHashLikeField(manifestHash, "manifestHash");

    storage::ChainManifest manifest(
        chainVersion,
        blockCount,
        genesisHash,
        latestHash,
        createdAt,
        manifestHash
    );

    if (!manifest.isValid()) {
        throw std::invalid_argument("Deserialized ChainManifest is invalid.");
    }

    if (manifest.serialize() != serialized) {
        throw std::logic_error("ChainManifest round-trip serialization mismatch.");
    }

    return manifest;
}

std::size_t ChainManifestCodec::parseSize(
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

std::int64_t ChainManifestCodec::parseSigned64(
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

void ChainManifestCodec::assertSafeHashLikeField(
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

} // namespace nodo::serialization