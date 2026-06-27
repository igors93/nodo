#include "core/LedgerRecordDomainValidator.hpp"

#include "economics/GenesisRewardRecord.hpp"
#include "economics/MintRecord.hpp"
#include "economics/ValidatorPenaltyRecord.hpp"

#include <exception>
#include <string>

namespace nodo::core {

namespace {

constexpr std::size_t kMaxPayloadBytes = 64 * 1024;

bool hasPrefix(const std::string& s, const char* prefix) {
    const std::size_t n = __builtin_strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

LedgerRecordDomainValidator::Result checkFormatPrefix(
    const std::string& payload,
    const char* expectedPrefix,
    const char* typeName
) {
    if (payload.empty() || payload.size() > kMaxPayloadBytes) {
        return LedgerRecordDomainValidator::Result::fail(
            std::string(typeName) + " ledger record payload is empty or too large."
        );
    }
    if (!hasPrefix(payload, expectedPrefix)) {
        return LedgerRecordDomainValidator::Result::fail(
            std::string(typeName) + " ledger record payload has unexpected format."
        );
    }
    return LedgerRecordDomainValidator::Result::ok();
}

} // namespace

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateMint(
    const LedgerRecord& record
) {
    try {
        const economics::MintRecord mint =
            economics::MintRecord::deserialize(record.payload());
        if (!mint.isValid()) {
            return Result::fail(
                "MINT ledger record payload deserializes to an invalid MintRecord: " +
                mint.rejectionReason()
            );
        }
    } catch (const std::exception& e) {
        return Result::fail(
            std::string("MINT ledger record payload failed deserialization: ") + e.what()
        );
    }
    return Result::ok();
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateValidationWork(
    const LedgerRecord& record
) {
    return checkFormatPrefix(record.payload(), "ValidationWorkRecord{", "VALIDATION_WORK");
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateValidatorScore(
    const LedgerRecord& record
) {
    return checkFormatPrefix(record.payload(), "ValidatorScoreRecord{", "VALIDATOR_SCORE");
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateProtectionEpoch(
    const LedgerRecord& record
) {
    return checkFormatPrefix(record.payload(), "ProtectionEpoch{", "PROTECTION_EPOCH");
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateGenesisReward(
    const LedgerRecord& record
) {
    try {
        const economics::GenesisRewardRecord reward =
            economics::GenesisRewardRecord::deserialize(record.payload());
        if (!reward.isValid()) {
            return Result::fail(
                "GENESIS_REWARD ledger record payload deserializes to an invalid GenesisRewardRecord."
            );
        }
    } catch (const std::exception& e) {
        return Result::fail(
            std::string("GENESIS_REWARD ledger record payload failed deserialization: ") + e.what()
        );
    }
    return Result::ok();
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validateValidatorPenalty(
    const LedgerRecord& record
) {
    try {
        const economics::ValidatorPenaltyRecord penalty =
            economics::ValidatorPenaltyRecord::deserialize(record.payload());
        if (!penalty.isValid()) {
            return Result::fail(
                "VALIDATOR_PENALTY ledger record payload deserializes to an invalid ValidatorPenaltyRecord."
            );
        }
    } catch (const std::exception& e) {
        return Result::fail(
            std::string("VALIDATOR_PENALTY ledger record payload failed deserialization: ") + e.what()
        );
    }
    return Result::ok();
}

// static
LedgerRecordDomainValidator::Result LedgerRecordDomainValidator::validate(
    const LedgerRecord& record
) {
    switch (record.type()) {
        case LedgerRecordType::MINT:
            return validateMint(record);
        case LedgerRecordType::VALIDATION_WORK:
            return validateValidationWork(record);
        case LedgerRecordType::VALIDATOR_SCORE:
            return validateValidatorScore(record);
        case LedgerRecordType::PROTECTION_EPOCH:
            return validateProtectionEpoch(record);
        case LedgerRecordType::GENESIS_REWARD:
            return validateGenesisReward(record);
        case LedgerRecordType::VALIDATOR_PENALTY:
            return validateValidatorPenalty(record);
        case LedgerRecordType::TRANSACTION:
            return Result::fail("TRANSACTION records must not be validated via domain validator.");
        default:
            return Result::fail("Unknown ledger record type encountered during domain validation.");
    }
}

} // namespace nodo::core
