#include "core/LedgerRecord.hpp"
#include "core/LedgerRecordDomainValidator.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/MintRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorPenaltyRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"
#include "crypto/hash.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace nodo;
using namespace nodo::core;
using namespace nodo::economics;

namespace {

constexpr std::int64_t kTs = 1700000200;

void require(bool condition, const char* msg) {
    if (!condition) {
        throw std::runtime_error(msg);
    }
}

// Builds a LedgerRecord by injecting a raw payload under an arbitrary type.
// Used to test failure cases where the payload is syntactically wrong.
LedgerRecord buildRawRecord(
    LedgerRecordType type,
    const std::string& sourceIdPrefix,
    const std::string& payload
) {
    char hashBuf[NODO_HASH_BUFFER_SIZE] = {};
    nodo_hash_string(payload.c_str(), hashBuf, sizeof(hashBuf));
    const std::string payloadHash(hashBuf);

    const std::string sourceId = sourceIdPrefix + "-" + payloadHash.substr(0, 8);

    char idBuf[NODO_HASH_BUFFER_SIZE] = {};
    const std::string idInput = "LedgerRecordId{type=" +
        ledgerRecordTypeToString(type) + ";sourceId=" + sourceId +
        ";payloadHash=" + payloadHash + ";timestamp=" + std::to_string(kTs) + "}";
    nodo_hash_string(idInput.c_str(), idBuf, sizeof(idBuf));

    return LedgerRecord::fromPersistedFields(
        std::string(idBuf),
        type,
        sourceId,
        payload,
        payloadHash,
        kTs
    );
}

void test_valid_mint_passes() {
    MintRecord mint(
        "mint-id-1",
        "auth-id-1",
        "recipient-addr",
        utils::Amount::fromRawUnits(1000),
        MintReason::GENESIS_ALLOCATION,
        1,
        0,
        "genesis-block-hash",
        kTs
    );
    const LedgerRecord record = LedgerRecord::fromMintRecord(mint, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid MintRecord should pass domain validation.");
}

void test_garbage_mint_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::MINT,
        "mint-src",
        "this is not a valid mint record"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage MINT payload should fail domain validation.");
}

void test_valid_validation_work_passes() {
    ValidationWorkRecord work(
        "validator-addr",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "target-hash-abc",
        "evidence-hash-xyz",
        1,
        kTs
    );
    const LedgerRecord record = LedgerRecord::fromValidationWorkRecord(work, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid ValidationWorkRecord should pass domain validation.");
}

void test_garbage_validation_work_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::VALIDATION_WORK,
        "vw-src",
        "not_a_validation_work_record"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage VALIDATION_WORK payload should fail domain validation.");
}

void test_valid_validator_score_passes() {
    ValidatorScoreRecord score(
        "validator-addr",
        1,
        50,
        75,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "evidence-hash",
        kTs
    );
    const LedgerRecord record = LedgerRecord::fromValidatorScoreRecord(score, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid ValidatorScoreRecord should pass domain validation.");
}

void test_garbage_validator_score_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::VALIDATOR_SCORE,
        "vs-src",
        "bad-score-data"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage VALIDATOR_SCORE payload should fail domain validation.");
}

void test_valid_protection_epoch_passes() {
    ProtectionEpoch epoch(
        1,
        0,
        99,
        utils::Amount::fromRawUnits(500),
        utils::Amount::fromRawUnits(1000),
        5000
    );
    const LedgerRecord record = LedgerRecord::fromProtectionEpoch(epoch, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid ProtectionEpoch should pass domain validation.");
}

void test_garbage_protection_epoch_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::PROTECTION_EPOCH,
        "pe-src",
        "garbage epoch data"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage PROTECTION_EPOCH payload should fail domain validation.");
}

void test_valid_genesis_reward_passes() {
    GenesisRewardRecord reward(
        1,
        "validator-addr",
        utils::Amount::fromRawUnits(2000),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-hash",
        "policy-v1",
        "accepted-block-hash",
        kTs
    );
    const LedgerRecord record = LedgerRecord::fromGenesisRewardRecord(reward, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid GenesisRewardRecord should pass domain validation.");
}

void test_garbage_genesis_reward_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::GENESIS_REWARD,
        "gr-src",
        "not a genesis reward"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage GENESIS_REWARD payload should fail domain validation.");
}

void test_valid_validator_penalty_passes() {
    ValidatorPenaltyRecord penalty(
        "validator-addr",
        1,
        10,
        75,
        25,
        ValidatorPenaltyReason::DOUBLE_SIGN,
        ValidatorPenaltyAction::SCORE_REDUCTION,
        "evidence-hash",
        "first-block-hash",
        "conflicting-block-hash",
        "first-sig-digest",
        "conflicting-sig-digest",
        kTs
    );
    const LedgerRecord record = LedgerRecord::fromValidatorPenaltyRecord(penalty, kTs);
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(result.valid, "Valid ValidatorPenaltyRecord should pass domain validation.");
}

void test_garbage_validator_penalty_payload_fails() {
    const LedgerRecord record = buildRawRecord(
        LedgerRecordType::VALIDATOR_PENALTY,
        "vp-src",
        "bad penalty data"
    );
    const auto result = LedgerRecordDomainValidator::validate(record);
    require(!result.valid, "Garbage VALIDATOR_PENALTY payload should fail domain validation.");
}

} // namespace

int main() {
    try {
        test_valid_mint_passes();
        test_garbage_mint_payload_fails();
        test_valid_validation_work_passes();
        test_garbage_validation_work_payload_fails();
        test_valid_validator_score_passes();
        test_garbage_validator_score_payload_fails();
        test_valid_protection_epoch_passes();
        test_garbage_protection_epoch_payload_fails();
        test_valid_genesis_reward_passes();
        test_garbage_genesis_reward_payload_fails();
        test_valid_validator_penalty_passes();
        test_garbage_validator_penalty_payload_fails();

        std::cout << "ledger record domain validator tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
