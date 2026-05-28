#include "serialization/MintRecordCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <stdexcept>

namespace nodo::serialization {

economics::MintRecord MintRecordCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("MintRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a MintRecord.");
    }

    economics::MintRecord record(
        FieldCodec::extractField(serialized, "id"),
        FieldCodec::extractField(serialized, "recipient"),
        utils::Amount::fromRawUnits(
            parseSigned64(
                FieldCodec::extractField(serialized, "amountRaw"),
                "amountRaw"
            )
        ),
        economics::mintReasonFromString(
            FieldCodec::extractField(serialized, "reason")
        ),
        parseUnsigned64(
            FieldCodec::extractField(serialized, "epoch"),
            "epoch"
        ),
        parseUnsigned64(
            FieldCodec::extractField(serialized, "sourceBlockIndex"),
            "sourceBlockIndex"
        ),
        FieldCodec::extractField(serialized, "sourceBlockHash"),
        parseSigned64(
            FieldCodec::extractField(serialized, "timestamp"),
            "timestamp"
        )
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized MintRecord is invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::logic_error("MintRecord round-trip serialization mismatch.");
    }

    return record;
}

std::int64_t MintRecordCodec::parseSigned64(
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

std::uint64_t MintRecordCodec::parseUnsigned64(
    const std::string& value,
    const std::string& fieldName
) {
    try {
        if (!value.empty() && value.front() == '-') {
            throw std::invalid_argument("Negative unsigned integer");
        }

        std::size_t parsedCharacters = 0;
        const std::uint64_t parsed = static_cast<std::uint64_t>(
            std::stoull(value, &parsedCharacters)
        );

        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }

        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid unsigned integer field: " + fieldName);
    }
}

} // namespace nodo::serialization