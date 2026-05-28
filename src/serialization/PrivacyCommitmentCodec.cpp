#include "serialization/PrivacyCommitmentCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <stdexcept>

namespace nodo::serialization {

privacy::PrivacyCommitment PrivacyCommitmentCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("PrivacyCommitment{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivacyCommitment.");
    }

    const std::string id =
        FieldCodec::extractField(serialized, "id");

    const privacy::PrivacyCommitmentType type =
        parsePrivacyCommitmentType(
            FieldCodec::extractField(serialized, "type")
        );

    const std::string commitmentHash =
        FieldCodec::extractField(serialized, "commitmentHash");

    const std::string ownerHint =
        FieldCodec::extractField(serialized, "ownerHint");

    const std::string sourceReference =
        FieldCodec::extractField(serialized, "sourceReference");

    const std::int64_t timestamp =
        parseSigned64(
            FieldCodec::extractField(serialized, "timestamp"),
            "timestamp"
        );

    /*
     * These are development-format checks. They are intentionally conservative
     * for identifiers and hash-like fields before the object is reconstructed.
     */
    assertSafeHexLikeField(id, "id");
    assertSafeHexLikeField(commitmentHash, "commitmentHash");
    assertSafeHexLikeField(ownerHint, "ownerHint");

    privacy::PrivacyCommitment commitment(
        id,
        type,
        commitmentHash,
        ownerHint,
        sourceReference,
        timestamp
    );

    if (!commitment.isValid()) {
        throw std::invalid_argument("Deserialized PrivacyCommitment is invalid.");
    }

    if (commitment.serialize() != serialized) {
        throw std::logic_error("PrivacyCommitment round-trip serialization mismatch.");
    }

    return commitment;
}

std::vector<privacy::PrivacyCommitment> PrivacyCommitmentCodec::deserializeList(
    const std::string& serializedList
) {
    std::vector<privacy::PrivacyCommitment> commitments;

    for (const auto& serializedCommitment :
         FieldCodec::splitTopLevelObjects(serializedList, "PrivacyCommitment{")) {
        commitments.push_back(
            deserialize(serializedCommitment)
        );
    }

    return commitments;
}

privacy::PrivacyCommitmentType PrivacyCommitmentCodec::parsePrivacyCommitmentType(
    const std::string& value
) {
    if (value == "MINT_COMMITMENT") {
        return privacy::PrivacyCommitmentType::MINT_COMMITMENT;
    }

    if (value == "TRANSFER_OUTPUT_COMMITMENT") {
        return privacy::PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT;
    }

    if (value == "BURN_COMMITMENT") {
        return privacy::PrivacyCommitmentType::BURN_COMMITMENT;
    }

    throw std::invalid_argument("Unknown PrivacyCommitmentType: " + value);
}

std::int64_t PrivacyCommitmentCodec::parseSigned64(
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

void PrivacyCommitmentCodec::assertSafeHexLikeField(
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