#include "serialization/LedgerRecordCodec.hpp"

#include "serialization/FieldCodec.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::serialization {

core::LedgerRecord LedgerRecordCodec::deserialize(
    const std::string& serialized
) {
    if (serialized.rfind("LedgerRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a LedgerRecord.");
    }

    const std::string id =
        FieldCodec::extractField(serialized, "id");

    const core::LedgerRecordType type =
        parseLedgerRecordType(
            FieldCodec::extractField(serialized, "type")
        );

    const std::string sourceId =
        FieldCodec::extractField(serialized, "sourceId");

    const std::string payloadHash =
        FieldCodec::extractField(serialized, "payloadHash");

    const std::int64_t timestamp =
        std::stoll(FieldCodec::extractField(serialized, "timestamp"));

    const std::string payload =
        extractPayload(serialized);

    assertSafePayloadPrefixForType(type, payload);

    /*
     * LedgerRecord constructor is private. LedgerRecordCodec is a friend
     * because it is the controlled deserialization boundary.
     */
    core::LedgerRecord record(
        id,
        type,
        sourceId,
        payload,
        payloadHash,
        timestamp
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized LedgerRecord is invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::logic_error("LedgerRecord round-trip serialization mismatch.");
    }

    return record;
}

std::vector<core::LedgerRecord> LedgerRecordCodec::deserializeList(
    const std::string& serializedList
) {
    std::vector<core::LedgerRecord> records;

    for (const auto& serializedRecord :
         FieldCodec::splitTopLevelObjects(serializedList, "LedgerRecord{")) {
        records.push_back(
            deserialize(serializedRecord)
        );
    }

    return records;
}

std::vector<core::LedgerRecord> LedgerRecordCodec::deserializeListFromBlockHeaderPayload(
    const std::string& blockHeaderPayload
) {
    if (blockHeaderPayload.rfind("BlockHeader{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a BlockHeader.");
    }

    const std::string recordsList = FieldCodec::extractTrailingSection(
        blockHeaderPayload,
        ";records=[",
        "]}"
    );

    return deserializeList(recordsList);
}

core::LedgerRecordType LedgerRecordCodec::parseLedgerRecordType(
    const std::string& value
) {
    if (value == "MINT") {
        return core::LedgerRecordType::MINT;
    }

    if (value == "TRANSACTION") {
        return core::LedgerRecordType::TRANSACTION;
    }

    if (value == "PRIVATE_ACCOUNTING") {
        return core::LedgerRecordType::PRIVATE_ACCOUNTING;
    }

    throw std::invalid_argument("Unknown LedgerRecordType: " + value);
}

std::string LedgerRecordCodec::extractPayload(
    const std::string& serialized
) {
    const std::string marker = ";payload=";
    const std::size_t markerPosition = serialized.find(marker);

    if (markerPosition == std::string::npos) {
        throw std::invalid_argument("Serialized LedgerRecord is missing payload.");
    }

    const std::size_t payloadStart = markerPosition + marker.size();

    if (serialized.size() <= payloadStart + 1) {
        throw std::invalid_argument("Serialized LedgerRecord payload is empty.");
    }

    if (serialized.back() != '}') {
        throw std::invalid_argument("Serialized LedgerRecord is missing closing brace.");
    }

    std::string payload =
        serialized.substr(payloadStart, serialized.size() - payloadStart - 1);

    if (payload.empty()) {
        throw std::invalid_argument("Serialized LedgerRecord payload is empty.");
    }

    return payload;
}

void LedgerRecordCodec::assertSafePayloadPrefixForType(
    core::LedgerRecordType type,
    const std::string& payload
) {
    if (payload.empty()) {
        throw std::invalid_argument("LedgerRecord payload cannot be empty.");
    }

    switch (type) {
        case core::LedgerRecordType::MINT:
            if (payload.rfind("MintRecord{", 0) != 0) {
                throw std::invalid_argument("MINT LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::TRANSACTION:
            if (payload.rfind("Transaction{", 0) != 0) {
                throw std::invalid_argument("TRANSACTION LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::PRIVATE_ACCOUNTING:
            if (payload.rfind("PrivateAccountingRecord{", 0) != 0) {
                throw std::invalid_argument("PRIVATE_ACCOUNTING LedgerRecord payload type mismatch.");
            }
            return;

        default:
            throw std::invalid_argument("Unsupported LedgerRecordType.");
    }
}

} // namespace nodo::serialization
