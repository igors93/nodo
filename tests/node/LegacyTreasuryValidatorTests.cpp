#include "node/LegacyTreasuryValidator.hpp"
#include "economics/TreasuryAccount.hpp"
#include "node/FeeEconomics.hpp"
#include "node/ProtectionTreasury.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::node::GenesisTreasurySnapshot;
using nodo::node::LegacyTreasuryValidationStatus;
using nodo::node::LegacyTreasuryValidator;
using nodo::node::TreasuryFeeRecord;
using nodo::utils::Amount;

GenesisTreasurySnapshot activeSnapshot(Amount balance) {
    // isValid() requires: protectedReserve + protectionBudget == genesisTreasuryBalance
    // AND availableBalance == genesisTreasuryBalance.
    const std::int64_t half = balance.rawUnits() / 2;
    return GenesisTreasurySnapshot(
        "ACTIVE",
        "treasury-protocol-account",
        1,
        balance,
        Amount::fromRawUnits(half),                                    // protectedReserve
        Amount::fromRawUnits(balance.rawUnits() - half),               // protectionBudget
        balance,                                                        // availableBalance == genesisTreasuryBalance
        "GENESIS_TREASURY_PROTECTION_RESERVE"
    );
}

TreasuryAccount validTreasury(Amount balance) {
    return TreasuryAccount(
        "treasury-main", "treasury-protocol-account",
        balance, 0, false, ""
    );
}

// Consistent genesis snapshot and TreasuryAccount pass.
void testConsistentGenesisSnapshotAndAccountPass() {
    const Amount balance = Amount::fromRawUnits(1000000);
    const auto result = LegacyTreasuryValidator::validateGenesisConsistency(
        activeSnapshot(balance),
        validTreasury(balance)
    );
    assert(result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::PASS);
}

// Mismatched genesis snapshot and TreasuryAccount fail.
void testMismatchedGenesisSnapshotAndAccountFail() {
    const auto result = LegacyTreasuryValidator::validateGenesisConsistency(
        activeSnapshot(Amount::fromRawUnits(1000000)),
        validTreasury(Amount::fromRawUnits(999999))  // different
    );
    assert(!result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::FAIL);
    assert(result.reason().find("mismatch") != std::string::npos ||
           result.reason().find("balance") != std::string::npos);
}

// Not-evaluated snapshot returns NOT_EVALUATED.
void testNotEvaluatedSnapshotSkipped() {
    const auto result = LegacyTreasuryValidator::validateGenesisConsistency(
        GenesisTreasurySnapshot::notEvaluated(),
        validTreasury(Amount::fromRawUnits(100000))
    );
    assert(!result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::NOT_EVALUATED);
}

// Invalid TreasuryAccount fails.
void testInvalidTreasuryAccountFails() {
    const TreasuryAccount invalid;
    const auto result = LegacyTreasuryValidator::validateGenesisConsistency(
        activeSnapshot(Amount::fromRawUnits(100000)),
        invalid
    );
    assert(!result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::FAIL);
}

// Matching TreasuryFeeRecord passes.
void testTreasuryFeeRecordMatchPasses() {
    const Amount expected = Amount::fromRawUnits(5000);
    const TreasuryFeeRecord feeRecord(
        "ACTIVE", 1,
        "treasury-protocol-account",
        expected,
        "TREASURY_FEE_ALLOCATION",
        "fee-balance-digest"
    );

    const auto result = LegacyTreasuryValidator::validateTreasuryFeeRecord(
        feeRecord, expected
    );
    assert(result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::PASS);
}

// Mismatched TreasuryFeeRecord fails.
void testTreasuryFeeRecordMismatchFails() {
    const TreasuryFeeRecord feeRecord(
        "ACTIVE", 1,
        "treasury-protocol-account",
        Amount::fromRawUnits(5000),
        "TREASURY_FEE_ALLOCATION",
        "fee-balance-digest"
    );

    const auto result = LegacyTreasuryValidator::validateTreasuryFeeRecord(
        feeRecord, Amount::fromRawUnits(4999)  // expected differs
    );
    assert(!result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::FAIL);
    assert(result.reason().find("mismatch") != std::string::npos ||
           result.reason().find("amount") != std::string::npos);
}

// Not-evaluated TreasuryFeeRecord returns NOT_EVALUATED.
void testNotEvaluatedFeeRecordSkipped() {
    const auto result = LegacyTreasuryValidator::validateTreasuryFeeRecord(
        TreasuryFeeRecord::notEvaluated(),
        Amount::fromRawUnits(5000)
    );
    assert(!result.passed());
    assert(result.status() == LegacyTreasuryValidationStatus::NOT_EVALUATED);
}

} // namespace

int main() {
    testConsistentGenesisSnapshotAndAccountPass();
    testMismatchedGenesisSnapshotAndAccountFail();
    testNotEvaluatedSnapshotSkipped();
    testInvalidTreasuryAccountFails();
    testTreasuryFeeRecordMatchPasses();
    testTreasuryFeeRecordMismatchFails();
    testNotEvaluatedFeeRecordSkipped();
    return 0;
}
