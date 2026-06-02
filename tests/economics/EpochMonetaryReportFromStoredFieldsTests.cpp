#include "economics/EpochMonetaryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::EpochMonetaryReport;
using nodo::economics::MonetaryPolicy;
using nodo::utils::Amount;

MonetaryPolicy validPolicy() {
    return MonetaryPolicy::localnetDefault(
        "stored-fields-test", Amount::fromRawUnits(100000)
    );
}

// Valid stored fields produce a valid report.
void testFromStoredFieldsAcceptsValidFields() {
    const auto policy = validPolicy();
    const auto r = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        1, 5,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(99800),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),
        2, 0, 2
    );
    assert(r.isValid());
    assert(r.startBlock() == 1);
    assert(r.endBlock() == 5);
    assert(r.startingSupply() == Amount::fromRawUnits(100000));
    assert(r.endingSupply() == Amount::fromRawUnits(99800));
    assert(r.totalBurned() == Amount::fromRawUnits(200));
    assert(r.deltaCount() == 2);
}

// Arithmetic mismatch: starting + minted - burned != ending.
void testFromStoredFieldsRejectsArithmeticMismatch() {
    const auto policy = validPolicy();
    const auto r = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        1, 5,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(99999),  // wrong: 100000+0-200=99800, not 99999
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),
        2, 0, 2
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("arithmetic") != std::string::npos ||
           r.rejectionReason().find("mismatch") != std::string::npos);
}

// Invalid policy is rejected.
void testFromStoredFieldsRejectsInvalidPolicy() {
    const MonetaryPolicy bad;
    const auto r = EpochMonetaryReport::fromStoredFields(
        bad, "some-version", 0,
        1, 5,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(800),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),
        2, 0, 2
    );
    assert(!r.isValid());
}

// policyVersion mismatch is rejected.
void testFromStoredFieldsRejectsPolicyVersionMismatch() {
    const auto policy = validPolicy();
    const auto r = EpochMonetaryReport::fromStoredFields(
        policy, "wrong-policy-version", 0,
        1, 5,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(99800),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),
        2, 0, 2
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("policyVersion") != std::string::npos ||
           r.rejectionReason().find("mismatch") != std::string::npos);
}

// startBlock > endBlock is rejected.
void testFromStoredFieldsRejectsStartBlockAfterEndBlock() {
    const auto policy = validPolicy();
    const auto r = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        10, 5,  // startBlock > endBlock
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(99800),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),
        2, 0, 2
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("startBlock") != std::string::npos ||
           r.rejectionReason().find("endBlock") != std::string::npos);
}

// Arithmetic check catches burn larger than starting supply.
// Since Amount prevents negative construction, the arithmetic guard fires when
// computed ending (starting + minted - burned < 0) != stored ending.
void testFromStoredFieldsRejectsExcessiveBurn() {
    const auto policy = validPolicy();
    // starting=100, minted=0, burned=200 -> computed ending = -100
    // stored ending = 1 (can't be negative), so mismatch is detected.
    const auto r = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        1, 5,
        Amount::fromRawUnits(100),
        Amount::fromRawUnits(1),     // any non-negative value != -100
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(200),   // burned > starting
        1, 0, 1
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("arithmetic") != std::string::npos ||
           r.rejectionReason().find("mismatch") != std::string::npos);
}

} // namespace

int main() {
    testFromStoredFieldsAcceptsValidFields();
    testFromStoredFieldsRejectsArithmeticMismatch();
    testFromStoredFieldsRejectsInvalidPolicy();
    testFromStoredFieldsRejectsPolicyVersionMismatch();
    testFromStoredFieldsRejectsStartBlockAfterEndBlock();
    testFromStoredFieldsRejectsExcessiveBurn();
    return 0;
}
