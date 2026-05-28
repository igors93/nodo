#include "serialization/PrivacyNullifierCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <stdexcept>

namespace nodo::serialization {

privacy::PrivacyNullifier PrivacyNullifierCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("PrivacyNullifier{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivacyNullifier.");
    }

    const std::string id =
        FieldCodec::extractField(serialized, "id");

    const privacy::PrivacyNullifierType type =
        parsePrivacyNullifierType(
            FieldCodec::extractField(serialized, "type")
        );

    const std::string nullifierHash =
        FieldCodec::extractField(serialized, "nullifierHash");

    const std::string contextHash =
        FieldCodec::extractField(serialized, "contextHash");

    const std::int64_t createdAt =
        parseSigned64(
            FieldCodec::extractField(serialized, "createdAt"),
            "createdAt"
        );

    /*
     * These are development-format checks. They are intentionally conservative
     * for identifiers and hash-like fields before object reconstruction.
     */
    assertSafeHexLikeField(id, "id");
    assertSafeHexLikeField(nullifierHash, "nullifierHash");
    assertSafeHexLikeField(contextHash, "contextHash");

    privacy::PrivacyNullifier nullifier(
        id,
        type,
        nullifierHash,
        contextHash,
        createdAt
    );

    if (!nullifier.isValid()) {
        throw std::invalid_argument("Deserialized PrivacyNullifier is invalid.");
    }

    if (nullifier.serialize() != serialized) {
        throw std::logic_error("PrivacyNullifier round-trip serialization mismatch.");
    }

    return nullifier;
}

std::vector<privacy::PrivacyNullifier> PrivacyNullifierCodec::deserializeList(
    const std::string& serializedList
) {
    std::vector<privacy::PrivacyNullifier> nullifiers;

    for (const auto& serializedNullifier :
         FieldCodec::splitTopLevelObjects(serializedList, "PrivacyNullifier{")) {
        nullifiers.push_back(
            deserialize(serializedNullifier)
        );
    }

    return nullifiers;
}

privacy::PrivacyNullifierType PrivacyNullifierCodec::parsePrivacyNullifierType(
    const std::string& value
) {
    if (value == "SPEND_NULLIFIER") {
        return privacy::PrivacyNullifierType::SPEND_NULLIFIER;
    }

    if (value == "BURN_NULLIFIER") {
        return privacy::PrivacyNullifierType::BURN_NULLIFIER;
    }

    throw std::invalid_argument("Unknown PrivacyNullifierType: " + value);
}

std::int64_t PrivacyNullifierCodec::parseSigned64(
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

void PrivacyNullifierCodec::assertSafeHexLikeField(
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