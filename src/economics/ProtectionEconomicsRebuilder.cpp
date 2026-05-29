#include "economics/ProtectionEconomicsRebuilder.hpp"

#include "serialization/FieldCodec.hpp"

#include <stdexcept>

namespace nodo::economics {

ProtectionEconomicsState ProtectionEconomicsRebuilder::rebuildFromBlockchain(
    const core::Blockchain& blockchain
) {
    if (!blockchain.isValid()) {
        throw std::invalid_argument("Cannot rebuild protection economy from invalid blockchain.");
    }

    return rebuildFromBlocks(blockchain.blocks());
}

ProtectionEconomicsState ProtectionEconomicsRebuilder::rebuildFromBlocks(
    const std::vector<core::Block>& blocks
) {
    ProtectionEconomicsState state;

    for (const auto& block : blocks) {
        if (!block.isValid()) {
            throw std::invalid_argument("Cannot rebuild protection economy from invalid block.");
        }

        for (const auto& record : block.records()) {
            applyLedgerRecord(
                state,
                record,
                block.index()
            );
        }
    }

    if (!state.isValid()) {
        throw std::logic_error("Protection economy rebuild produced invalid state.");
    }

    return state;
}

void ProtectionEconomicsRebuilder::applyLedgerRecord(
    ProtectionEconomicsState& state,
    const core::LedgerRecord& record,
    std::uint64_t blockIndex
) {
    if (!record.isValid()) {
        throw std::invalid_argument("Cannot apply invalid LedgerRecord to protection economy.");
    }

    if (!isProtectionRecord(record.type())) {
        return;
    }

    switch (record.type()) {
        case core::LedgerRecordType::VALIDATION_WORK:
            applyValidationWorkRecord(state, record);
            return;

        case core::LedgerRecordType::VALIDATOR_SCORE:
            applyValidatorScoreRecord(state, record);
            return;

        case core::LedgerRecordType::PROTECTION_EPOCH:
            applyProtectionEpochRecord(state, record);
            return;

        case core::LedgerRecordType::GENESIS_REWARD:
            applyGenesisRewardRecord(state, record, blockIndex);
            return;

        default:
            return;
    }
}

bool ProtectionEconomicsRebuilder::isProtectionRecord(
    core::LedgerRecordType type
) {
    return type == core::LedgerRecordType::VALIDATION_WORK ||
           type == core::LedgerRecordType::VALIDATOR_SCORE ||
           type == core::LedgerRecordType::PROTECTION_EPOCH ||
           type == core::LedgerRecordType::GENESIS_REWARD;
}

std::int64_t ProtectionEconomicsRebuilder::parseInt64Field(
    const std::string& payload,
    const std::string& fieldName
) {
    return std::stoll(
        parseStringField(
            payload,
            fieldName
        )
    );
}

std::uint64_t ProtectionEconomicsRebuilder::parseUInt64Field(
    const std::string& payload,
    const std::string& fieldName
) {
    return static_cast<std::uint64_t>(
        std::stoull(
            parseStringField(
                payload,
                fieldName
            )
        )
    );
}

std::int32_t ProtectionEconomicsRebuilder::parseInt32Field(
    const std::string& payload,
    const std::string& fieldName
) {
    return static_cast<std::int32_t>(
        std::stoi(
            parseStringField(
                payload,
                fieldName
            )
        )
    );
}

std::string ProtectionEconomicsRebuilder::parseStringField(
    const std::string& payload,
    const std::string& fieldName
) {
    return serialization::FieldCodec::extractField(
        payload,
        fieldName
    );
}

void ProtectionEconomicsRebuilder::applyValidationWorkRecord(
    ProtectionEconomicsState& state,
    const core::LedgerRecord& record
) {
    const std::string& payload = record.payload();

    if (payload.rfind("ValidationWorkRecord{", 0) != 0) {
        throw std::invalid_argument("LedgerRecord payload is not a ValidationWorkRecord.");
    }

    const std::string result =
        parseStringField(
            payload,
            "result"
        );

    if (result != "ACCEPTED") {
        return;
    }

    state.applyAcceptedWork(
        parseStringField(payload, "validator"),
        parseUInt64Field(payload, "workWeight")
    );
}

void ProtectionEconomicsRebuilder::applyValidatorScoreRecord(
    ProtectionEconomicsState& state,
    const core::LedgerRecord& record
) {
    const std::string& payload = record.payload();

    if (payload.rfind("ValidatorScoreRecord{", 0) != 0) {
        throw std::invalid_argument("LedgerRecord payload is not a ValidatorScoreRecord.");
    }

    state.applyValidatorScore(
        parseStringField(payload, "validator"),
        parseInt32Field(payload, "newScore")
    );
}

void ProtectionEconomicsRebuilder::applyProtectionEpochRecord(
    ProtectionEconomicsState& state,
    const core::LedgerRecord& record
) {
    const std::string& payload = record.payload();

    if (payload.rfind("ProtectionEpoch{", 0) != 0) {
        throw std::invalid_argument("LedgerRecord payload is not a ProtectionEpoch.");
    }

    state.applyProtectionEpochTotals(
        utils::Amount::fromRawUnits(
            parseInt64Field(payload, "securityEmissionRaw")
        ),
        utils::Amount::fromRawUnits(
            parseInt64Field(payload, "rewardPoolRaw")
        )
    );
}

void ProtectionEconomicsRebuilder::applyGenesisRewardRecord(
    ProtectionEconomicsState& state,
    const core::LedgerRecord& record,
    std::uint64_t blockIndex
) {
    const std::string& payload = record.payload();

    if (payload.rfind("GenesisRewardRecord{", 0) != 0) {
        throw std::invalid_argument("LedgerRecord payload is not a GenesisRewardRecord.");
    }

    state.applyGenesisReward(
        record.sourceId(),
        parseStringField(payload, "validator"),
        utils::Amount::fromRawUnits(
            parseInt64Field(payload, "amountRaw")
        ),
        blockIndex,
        parseInt64Field(payload, "timestamp")
    );
}

} // namespace nodo::economics
