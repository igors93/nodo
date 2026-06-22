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

    if (value == "VALIDATION_WORK") {
        return core::LedgerRecordType::VALIDATION_WORK;
    }

    if (value == "VALIDATOR_SCORE") {
        return core::LedgerRecordType::VALIDATOR_SCORE;
    }

    if (value == "PROTECTION_EPOCH") {
        return core::LedgerRecordType::PROTECTION_EPOCH;
    }

    if (value == "GENESIS_REWARD") {
        return core::LedgerRecordType::GENESIS_REWARD;
    }

    if (value == "VALIDATOR_PENALTY") {
        return core::LedgerRecordType::VALIDATOR_PENALTY;
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

        case core::LedgerRecordType::VALIDATION_WORK:
            if (payload.rfind("ValidationWorkRecord{", 0) != 0) {
                throw std::invalid_argument("VALIDATION_WORK LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::VALIDATOR_SCORE:
            if (payload.rfind("ValidatorScoreRecord{", 0) != 0) {
                throw std::invalid_argument("VALIDATOR_SCORE LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::PROTECTION_EPOCH:
            if (payload.rfind("ProtectionEpoch{", 0) != 0) {
                throw std::invalid_argument("PROTECTION_EPOCH LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::GENESIS_REWARD:
            if (payload.rfind("GenesisRewardRecord{", 0) != 0) {
                throw std::invalid_argument("GENESIS_REWARD LedgerRecord payload type mismatch.");
            }
            return;

        case core::LedgerRecordType::VALIDATOR_PENALTY:
            if (payload.rfind("ValidatorPenaltyRecord{", 0) != 0) {
                throw std::invalid_argument("VALIDATOR_PENALTY LedgerRecord payload type mismatch.");
            }
            return;

        default:
            throw std::invalid_argument("Unsupported LedgerRecordType.");
    }
}

} // namespace nodo::serialization
