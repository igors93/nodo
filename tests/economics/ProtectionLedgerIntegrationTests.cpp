#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "serialization/LedgerRecordCodec.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::LedgerRecord;
using nodo::core::LedgerRecordType;
using nodo::economics::GenesisRewardReason;
using nodo::economics::GenesisRewardRecord;
using nodo::economics::ProtectionEpoch;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::economics::ValidatorScoreReason;
using nodo::economics::ValidatorScoreRecord;
using nodo::serialization::LedgerRecordCodec;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

ValidationWorkRecord sampleWorkRecord() {
    return ValidationWorkRecord(
        "nodo1validator",
        1,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::ACCEPTED,
        "coin-lot-target-hash",
        "coin-lot-evidence-hash",
        25,
        kTimestamp
    );
}

ValidatorScoreRecord sampleScoreRecord() {
    return ValidatorScoreRecord(
        "nodo1validator",
        1,
        50,
        55,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "validator-score-evidence",
        kTimestamp
    );
}

ProtectionEpoch sampleProtectionEpoch() {
    return ProtectionEpoch(
        1,
        0,
        10,
        Amount::fromNodo(5),
        Amount::fromNodo(100),
        7500
    );
}

GenesisRewardRecord sampleGenesisRewardRecord() {
    return GenesisRewardRecord(
        1,
        "nodo1validator",
        Amount::fromNodo(30),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-hash",
        "NODO_EPOCH_EMISSION_POLICY_V1",
        "accepted-block-hash",
        kTimestamp
    );
}

void assertRoundTrip(
    const LedgerRecord& record
) {
    requireCondition(
        record.isValid(),
        "LedgerRecord should be valid before round-trip."
    );

    const LedgerRecord loaded =
        LedgerRecordCodec::deserialize(
            record.serialize()
        );

    requireCondition(
        loaded.serialize() == record.serialize(),
        "LedgerRecord round-trip serialization changed the record."
    );

    requireCondition(
        loaded.isValid(),
        "LedgerRecord should be valid after round-trip."
    );
}

void testValidationWorkLedgerRecord() {
    const LedgerRecord record =
        LedgerRecord::fromValidationWorkRecord(
            sampleWorkRecord(),
            kTimestamp
        );

    requireCondition(
        record.type() == LedgerRecordType::VALIDATION_WORK,
        "ValidationWorkRecord ledger type is wrong."
    );

    requireCondition(
        record.payload().rfind("ValidationWorkRecord{", 0) == 0,
        "ValidationWorkRecord payload prefix is wrong."
    );

    assertRoundTrip(record);
}

void testValidatorScoreLedgerRecord() {
    const LedgerRecord record =
        LedgerRecord::fromValidatorScoreRecord(
            sampleScoreRecord(),
            kTimestamp
        );

    requireCondition(
        record.type() == LedgerRecordType::VALIDATOR_SCORE,
        "ValidatorScoreRecord ledger type is wrong."
    );

    requireCondition(
        record.payload().rfind("ValidatorScoreRecord{", 0) == 0,
        "ValidatorScoreRecord payload prefix is wrong."
    );

    assertRoundTrip(record);
}

void testProtectionEpochLedgerRecord() {
    const LedgerRecord record =
        LedgerRecord::fromProtectionEpoch(
            sampleProtectionEpoch(),
            kTimestamp
        );

    requireCondition(
        record.type() == LedgerRecordType::PROTECTION_EPOCH,
        "ProtectionEpoch ledger type is wrong."
    );

    requireCondition(
        record.payload().rfind("ProtectionEpoch{", 0) == 0,
        "ProtectionEpoch payload prefix is wrong."
    );

    assertRoundTrip(record);
}

void testGenesisRewardLedgerRecord() {
    const GenesisRewardRecord reward =
        sampleGenesisRewardRecord();

    const LedgerRecord record =
        LedgerRecord::fromGenesisRewardRecord(
            reward,
            kTimestamp
        );

    requireCondition(
        record.type() == LedgerRecordType::GENESIS_REWARD,
        "GenesisRewardRecord ledger type is wrong."
    );

    requireCondition(
        record.sourceId() == reward.deterministicId(),
        "GenesisRewardRecord ledger source id should be the reward id."
    );

    requireCondition(
        record.payload().rfind("GenesisRewardRecord{", 0) == 0,
        "GenesisRewardRecord payload prefix is wrong."
    );

    assertRoundTrip(record);
}

void testProtectionLedgerRecordsCanEnterBlock() {
    const std::vector<LedgerRecord> records = {
        LedgerRecord::fromValidationWorkRecord(
            sampleWorkRecord(),
            kTimestamp
        ),
        LedgerRecord::fromValidatorScoreRecord(
            sampleScoreRecord(),
            kTimestamp + 1
        ),
        LedgerRecord::fromProtectionEpoch(
            sampleProtectionEpoch(),
            kTimestamp + 2
        ),
        LedgerRecord::fromGenesisRewardRecord(
            sampleGenesisRewardRecord(),
            kTimestamp + 3
        )
    };

    const Block block(
        1,
        "previous-block-hash",
        records,
        kTimestamp + 4
    );

    requireCondition(
        block.isValid(),
        "Block containing protection ledger records should be valid."
    );

    requireCondition(
        block.records().size() == 4U,
        "Block should contain four protection ledger records."
    );
}

void testCodecRejectsMismatchedProtectionPayload() {
    const LedgerRecord record =
        LedgerRecord::fromGenesisRewardRecord(
            sampleGenesisRewardRecord(),
            kTimestamp
        );

    std::string tampered = record.serialize();

    const std::string from = ";type=GENESIS_REWARD;";
    const std::string to = ";type=VALIDATION_WORK;";

    const std::size_t position = tampered.find(from);

    requireCondition(
        position != std::string::npos,
        "Could not find ledger type marker for tamper test."
    );

    tampered.replace(
        position,
        from.size(),
        to
    );

    bool rejected = false;

    try {
        (void)LedgerRecordCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "LedgerRecordCodec accepted a mismatched protection payload."
    );
}

} // namespace

int main() {
    try {
        testValidationWorkLedgerRecord();
        testValidatorScoreLedgerRecord();
        testProtectionEpochLedgerRecord();
        testGenesisRewardLedgerRecord();
        testProtectionLedgerRecordsCanEnterBlock();
        testCodecRejectsMismatchedProtectionPayload();

        std::cout << "Nodo protection ledger integration tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protection ledger integration tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
