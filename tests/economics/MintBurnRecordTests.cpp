#include "economics/BurnRecord.hpp"
#include "economics/MintRecord.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

// ---- MintRecord tests ----

nodo::economics::MintRecord validMint() {
    return nodo::economics::MintRecord(
        "mint-001",
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(500),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1,
        10,
        "block-hash-abcdef",
        1900000001
    );
}

void testValidMintRecordIsValid() {
    const auto mint = validMint();
    assert(mint.isValid());
    assert(mint.id() == "mint-001");
    assert(mint.recipientAddress() == "nodo1recipient001");
    assert(mint.amount() == nodo::utils::Amount::fromRawUnits(500));
    assert(mint.reason() == nodo::economics::MintReason::GENESIS_ALLOCATION);
}

void testMintWithZeroAmountIsInvalid() {
    const nodo::economics::MintRecord mint(
        "mint-zero",
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(0),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 10, "block-hash", 1900000001
    );
    assert(!mint.isValid());
}

void testMintWithEmptyIdIsInvalid() {
    const nodo::economics::MintRecord mint(
        "",
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 10, "block-hash", 1900000001
    );
    assert(!mint.isValid());
}

void testMintWithEmptyRecipientIsInvalid() {
    const nodo::economics::MintRecord mint(
        "mint-002",
        "",
        nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 10, "block-hash", 1900000001
    );
    assert(!mint.isValid());
}

void testMintWithEmptySourceBlockHashIsInvalid() {
    const nodo::economics::MintRecord mint(
        "mint-003",
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 10, "", 1900000001
    );
    assert(!mint.isValid());
}

void testMintReasonRoundtrip() {
    assert(nodo::economics::mintReasonToString(
               nodo::economics::MintReason::GENESIS_ALLOCATION) == "GENESIS_ALLOCATION");
    assert(nodo::economics::mintReasonFromString("GENESIS_ALLOCATION") ==
           nodo::economics::MintReason::GENESIS_ALLOCATION);
    assert(nodo::economics::mintReasonToString(
               nodo::economics::MintReason::NETWORK_DEFENSE_REWARD) == "NETWORK_DEFENSE_REWARD");
}

// ---- BurnRecord tests ----

nodo::economics::BurnRecord validBurn() {
    return nodo::economics::BurnRecord(
        "burn-001",
        10,
        1,
        "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(100),
        "transaction fee",
        nodo::economics::BurnType::FEE_BURN
    );
}

void testValidBurnRecordIsValid() {
    const auto burn = validBurn();
    assert(burn.isValid());
    assert(burn.rejectionReason().empty());
    assert(burn.burnId() == "burn-001");
    assert(burn.sourceAddress() == "nodo1sender001");
    assert(burn.amount() == nodo::utils::Amount::fromRawUnits(100));
    assert(burn.burnType() == nodo::economics::BurnType::FEE_BURN);
}

void testBurnWithEmptyIdIsInvalid() {
    const nodo::economics::BurnRecord burn(
        "", 10, 1, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(100), "fee", nodo::economics::BurnType::FEE_BURN
    );
    assert(!burn.isValid());
    assert(!burn.rejectionReason().empty());
}

void testBurnWithZeroAmountIsInvalid() {
    const nodo::economics::BurnRecord burn(
        "burn-z", 10, 1, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(0), "fee", nodo::economics::BurnType::FEE_BURN
    );
    assert(!burn.isValid());
}

void testBurnWithEmptySourceIsInvalid() {
    const nodo::economics::BurnRecord burn(
        "burn-002", 10, 1, "",
        nodo::utils::Amount::fromRawUnits(100), "fee", nodo::economics::BurnType::FEE_BURN
    );
    assert(!burn.isValid());
}

void testBurnWithEmptyReasonIsInvalid() {
    const nodo::economics::BurnRecord burn(
        "burn-003", 10, 1, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(100), "", nodo::economics::BurnType::FEE_BURN
    );
    assert(!burn.isValid());
}

void testBurnTypeRoundtrip() {
    assert(nodo::economics::burnTypeToString(nodo::economics::BurnType::FEE_BURN) == "FEE_BURN");
    assert(nodo::economics::burnTypeFromString("SLASH_BURN") == nodo::economics::BurnType::SLASH_BURN);
    assert(nodo::economics::burnTypeToString(nodo::economics::BurnType::PENALTY_BURN) == "PENALTY_BURN");
    assert(nodo::economics::burnTypeToString(
               nodo::economics::BurnType::GOVERNANCE_DEPOSIT_BURN) == "GOVERNANCE_DEPOSIT_BURN");
    assert(nodo::economics::burnTypeToString(
               nodo::economics::BurnType::VOLUNTARY_BURN) == "VOLUNTARY_BURN");
}

void testBurnTypeFromStringUnknownThrows() {
    bool threw = false;
    try {
        nodo::economics::burnTypeFromString("UNKNOWN_TYPE");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testBurnSerializationContainsKeyFields() {
    const auto burn = validBurn();
    const std::string s = burn.serialize();
    assert(!s.empty());
    assert(s.find("burn-001") != std::string::npos);
    assert(s.find("FEE_BURN") != std::string::npos);
    assert(s.find("nodo1sender001") != std::string::npos);
}

} // namespace

int main() {
    testValidMintRecordIsValid();
    testMintWithZeroAmountIsInvalid();
    testMintWithEmptyIdIsInvalid();
    testMintWithEmptyRecipientIsInvalid();
    testMintWithEmptySourceBlockHashIsInvalid();
    testMintReasonRoundtrip();

    testValidBurnRecordIsValid();
    testBurnWithEmptyIdIsInvalid();
    testBurnWithZeroAmountIsInvalid();
    testBurnWithEmptySourceIsInvalid();
    testBurnWithEmptyReasonIsInvalid();
    testBurnTypeRoundtrip();
    testBurnTypeFromStringUnknownThrows();
    testBurnSerializationContainsKeyFields();
    return 0;
}
