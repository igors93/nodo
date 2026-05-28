#include "serialization/PrivateAccountingRecordCodec.hpp"

#include "serialization/FieldCodec.hpp"
#include "serialization/PrivacyCommitmentCodec.hpp"
#include "serialization/PrivacyNullifierCodec.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::serialization {

privacy::PrivateAccountingRecord PrivateAccountingRecordCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("PrivateAccountingRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivateAccountingRecord.");
    }

    const std::string id =
        FieldCodec::extractField(serialized, "id");

    const privacy::PrivateAccountingRecordType type =
        parsePrivateAccountingRecordType(
            FieldCodec::extractField(serialized, "type")
        );

    const privacy::PublicSupplyEffect supplyEffect =
        parsePublicSupplyEffect(
            FieldCodec::extractField(serialized, "supplyEffect")
        );

    const utils::Amount publicSupplyAmount =
        utils::Amount::fromRawUnits(
            parseSigned64(
                FieldCodec::extractField(serialized, "publicSupplyAmountRaw"),
                "publicSupplyAmountRaw"
            )
        );

    const std::string auditReference =
        FieldCodec::extractField(serialized, "auditReference");

    const std::string proofHash =
        FieldCodec::extractField(serialized, "proofHash");

    const std::int64_t timestamp =
        parseSigned64(
            FieldCodec::extractField(serialized, "timestamp"),
            "timestamp"
        );

    const std::string inputNullifierList = FieldCodec::extractBetween(
        serialized,
        ";inputNullifiers=[",
        "];outputCommitments=["
    );

    const std::string outputCommitmentList = FieldCodec::extractTrailingSection(
        serialized,
        "];outputCommitments=[",
        "]}"
    );

    std::vector<privacy::PrivacyNullifier> inputNullifiers =
        PrivacyNullifierCodec::deserializeList(inputNullifierList);

    std::vector<privacy::PrivacyCommitment> outputCommitments =
        PrivacyCommitmentCodec::deserializeList(outputCommitmentList);

    /*
     * These are development-format checks. Final production serialization must
     * replace these text-format assumptions with canonical encoding rules.
     */
    assertSafeHexLikeField(id, "id");
    assertSafeHexLikeField(proofHash, "proofHash");

    privacy::PrivateAccountingRecord record(
        id,
        type,
        supplyEffect,
        publicSupplyAmount,
        std::move(inputNullifiers),
        std::move(outputCommitments),
        auditReference,
        proofHash,
        timestamp
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized PrivateAccountingRecord is invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::logic_error("PrivateAccountingRecord round-trip serialization mismatch.");
    }

    return record;
}

std::vector<privacy::PrivateAccountingRecord> PrivateAccountingRecordCodec::deserializeList(
    const std::string& serializedList
) {
    std::vector<privacy::PrivateAccountingRecord> records;

    for (const auto& serializedRecord :
         FieldCodec::splitTopLevelObjects(serializedList, "PrivateAccountingRecord{")) {
        records.push_back(
            deserialize(serializedRecord)
        );
    }

    return records;
}

privacy::PrivateAccountingRecordType
PrivateAccountingRecordCodec::parsePrivateAccountingRecordType(
    const std::string& value
) {
    if (value == "PRIVATE_MINT") {
        return privacy::PrivateAccountingRecordType::PRIVATE_MINT;
    }

    if (value == "PRIVATE_TRANSFER") {
        return privacy::PrivateAccountingRecordType::PRIVATE_TRANSFER;
    }

    if (value == "PRIVATE_BURN") {
        return privacy::PrivateAccountingRecordType::PRIVATE_BURN;
    }

    throw std::invalid_argument("Unknown PrivateAccountingRecordType: " + value);
}

privacy::PublicSupplyEffect PrivateAccountingRecordCodec::parsePublicSupplyEffect(
    const std::string& value
) {
    if (value == "NO_SUPPLY_CHANGE") {
        return privacy::PublicSupplyEffect::NO_SUPPLY_CHANGE;
    }

    if (value == "SUPPLY_INCREASE") {
        return privacy::PublicSupplyEffect::SUPPLY_INCREASE;
    }

    if (value == "SUPPLY_DECREASE") {
        return privacy::PublicSupplyEffect::SUPPLY_DECREASE;
    }

    throw std::invalid_argument("Unknown PublicSupplyEffect: " + value);
}

std::int64_t PrivateAccountingRecordCodec::parseSigned64(
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

void PrivateAccountingRecordCodec::assertSafeHexLikeField(
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