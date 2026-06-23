#include "mempool/FeeMarket.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::mempool::FeeMarket;
using nodo::mempool::FeeMarketState;
using nodo::mempool::FeeUrgency;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testFeeIncreasesWhenBlockIsFull() {
    // Target = 10, actual = 20 (double the target)
    const Amount current = Amount::fromRawUnits(1000);
    const Amount next = FeeMarket::computeNextBaseFee(current, 20, 10);

    requireCondition(
        next > current,
        "Fee should increase when block is over target capacity."
    );

    // Delta = min(125, 1000 * 10 / 10) = min(125, 1000) = 125
    // next = 1000 + 125 = 1125
    requireCondition(
        next.rawUnits() == 1125,
        "Fee increase should be capped at 12.5% when fully over target."
    );
}

void testFeeDecreasesWhenBlockIsEmpty() {
    const Amount current = Amount::fromRawUnits(1000);
    const Amount next = FeeMarket::computeNextBaseFee(current, 0, 10);

    requireCondition(
        next < current,
        "Fee should decrease when block is under target (empty)."
    );

    // deficit = 10, delta = min(125, 1000 * 10 / 10) = 125
    // next = 1000 - 125 = 875
    requireCondition(
        next.rawUnits() == 875,
        "Fee decrease should be capped at 12.5% when completely empty."
    );
}

void testFeeNeverGoesBelowMinimum() {
    // Start at the minimum itself and undershoot
    const Amount atMinimum = Amount::fromRawUnits(FeeMarket::MINIMUM_BASE_FEE_RAW);
    const Amount next = FeeMarket::computeNextBaseFee(atMinimum, 0, 10);

    requireCondition(
        next.rawUnits() >= FeeMarket::MINIMUM_BASE_FEE_RAW,
        "Fee should never drop below MINIMUM_BASE_FEE_RAW."
    );

    // Very high target deficit from a low base
    const Amount tiny = Amount::fromRawUnits(FeeMarket::MINIMUM_BASE_FEE_RAW);
    const Amount nextTiny = FeeMarket::computeNextBaseFee(tiny, 0, 1000);

    requireCondition(
        nextTiny.rawUnits() >= FeeMarket::MINIMUM_BASE_FEE_RAW,
        "Fee floor must hold even with severe under-utilization."
    );
}

void testFeeChangeIsCappedAt12Point5Percent() {
    const Amount current = Amount::fromRawUnits(10000);

    // Completely full (100x over target)
    const Amount afterFull = FeeMarket::computeNextBaseFee(current, 100, 1);
    const std::int64_t maxDelta =
        (10000 * FeeMarket::MAX_FEE_CHANGE_NUMERATOR)
        / FeeMarket::MAX_FEE_CHANGE_DENOMINATOR;

    requireCondition(
        afterFull.rawUnits() <= current.rawUnits() + maxDelta,
        "Fee increase must not exceed 12.5% cap."
    );

    // Completely empty
    const Amount afterEmpty = FeeMarket::computeNextBaseFee(current, 0, 100);

    requireCondition(
        afterEmpty.rawUnits() >= current.rawUnits() - maxDelta,
        "Fee decrease must not exceed 12.5% cap."
    );
}

void testFeeUnchangedAtTarget() {
    const Amount current = Amount::fromRawUnits(1000);
    const Amount next = FeeMarket::computeNextBaseFee(current, 10, 10);

    requireCondition(
        next.rawUnits() == current.rawUnits(),
        "Fee should remain unchanged when block hits exactly the target."
    );
}

void testEstimateFeeReturnsCorrectMultipliers() {
    const Amount base = Amount::fromRawUnits(1000);

    const Amount low    = FeeMarket::estimateFee(FeeUrgency::LOW, base);
    const Amount medium = FeeMarket::estimateFee(FeeUrgency::MEDIUM, base);
    const Amount high   = FeeMarket::estimateFee(FeeUrgency::HIGH, base);

    requireCondition(
        low.rawUnits() == 1000,
        "LOW urgency should return exactly baseFee (1.0x)."
    );

    requireCondition(
        medium.rawUnits() == 1100,
        "MEDIUM urgency should return baseFee * 1.1."
    );

    requireCondition(
        high.rawUnits() == 1250,
        "HIGH urgency should return baseFee * 1.25."
    );
}

void testIsFeeSufficientCorrectlyFilters() {
    const Amount baseFee = Amount::fromRawUnits(500);

    requireCondition(
        FeeMarket::isFeeSufficient(Amount::fromRawUnits(500), baseFee),
        "Exact base fee should be sufficient."
    );

    requireCondition(
        FeeMarket::isFeeSufficient(Amount::fromRawUnits(501), baseFee),
        "Fee above base should be sufficient."
    );

    requireCondition(
        !FeeMarket::isFeeSufficient(Amount::fromRawUnits(499), baseFee),
        "Fee below base should not be sufficient."
    );

    requireCondition(
        !FeeMarket::isFeeSufficient(Amount::fromRawUnits(0), baseFee),
        "Zero fee should not be sufficient when base > 0."
    );
}

void testAdvanceStateUpdatesBlockHeightAndTxCount() {
    const FeeMarketState initial = FeeMarket::initialState(10);

    requireCondition(
        initial.blockHeight == 0,
        "Initial state should have blockHeight = 0."
    );
    requireCondition(
        initial.baseFee.rawUnits() == FeeMarket::MINIMUM_BASE_FEE_RAW,
        "Initial state baseFee should equal the minimum."
    );

    const FeeMarketState next = FeeMarket::advanceState(initial, 5);

    requireCondition(
        next.blockHeight == 1,
        "Advancing state should increment blockHeight to 1."
    );
    requireCondition(
        next.lastBlockTxCount == 5,
        "advanceState should record actual tx count."
    );
    requireCondition(
        next.targetBlockCapacity == initial.targetBlockCapacity,
        "Target capacity should be preserved across state advance."
    );
}

void testInitialStateHasFloorFee() {
    const FeeMarketState state = FeeMarket::initialState(100);

    requireCondition(
        state.baseFee.rawUnits() == FeeMarket::MINIMUM_BASE_FEE_RAW,
        "Initial state baseFee must equal the minimum base fee."
    );
    requireCondition(
        state.targetBlockCapacity == 100,
        "Initial state must reflect the provided target capacity."
    );
    requireCondition(
        state.lastBlockTxCount == 0,
        "Initial state last tx count must be zero."
    );
}

void testSerializeProducesNonEmptyString() {
    const FeeMarketState state = FeeMarket::initialState(50);
    const std::string serialized = state.serialize();

    requireCondition(
        !serialized.empty(),
        "FeeMarketState::serialize() must produce a non-empty string."
    );
    requireCondition(
        serialized.find("FeeMarketState") != std::string::npos,
        "Serialized form should contain the type identifier."
    );
}

} // namespace

int main() {
    try {
        testFeeIncreasesWhenBlockIsFull();
        testFeeDecreasesWhenBlockIsEmpty();
        testFeeNeverGoesBelowMinimum();
        testFeeChangeIsCappedAt12Point5Percent();
        testFeeUnchangedAtTarget();
        testEstimateFeeReturnsCorrectMultipliers();
        testIsFeeSufficientCorrectlyFilters();
        testAdvanceStateUpdatesBlockHeightAndTxCount();
        testInitialStateHasFloorFee();
        testSerializeProducesNonEmptyString();

        std::cout << "Nodo FeeMarket tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo FeeMarket tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
