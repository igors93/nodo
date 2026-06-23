#include "consensus/ChainReorgGuard.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::consensus::ChainReorgGuard;
using nodo::consensus::ChainReorgGuardConfig;
using nodo::consensus::ChainReorgCheckResult;
using nodo::consensus::ReorgCheckOutcome;
using nodo::consensus::ReorgEvent;

constexpr std::int64_t kNow = 1900000000LL;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testConfigDefaultsAreValid() {
    const ChainReorgGuardConfig config = ChainReorgGuardConfig::defaults();

    requireCondition(
        config.isValid(),
        "Default ChainReorgGuardConfig must be valid."
    );
    requireCondition(
        config.maxReorgDepth == 6,
        "Default maxReorgDepth must be 6."
    );
    requireCondition(
        config.alertThresholdDepth == 3,
        "Default alertThresholdDepth must be 3."
    );
}

void testConfigInvalidWhenAlertThresholdExceedsMax() {
    ChainReorgGuardConfig bad;
    bad.maxReorgDepth       = 3;
    bad.alertThresholdDepth = 5;  // alert >= max → invalid

    requireCondition(
        !bad.isValid(),
        "Config with alertThreshold >= maxReorgDepth must be invalid."
    );
}

void testDepthWithinAlertThresholdIsAllowed() {
    ChainReorgGuard guard;

    // depth = 10 - 8 = 2, alert threshold = 3, max = 6
    const auto result = guard.checkReorg(10, 8, kNow);

    requireCondition(
        result.outcome() == ReorgCheckOutcome::ALLOWED,
        "depth=2 should produce ALLOWED outcome."
    );
    requireCondition(
        result.isAllowed(),
        "isAllowed() must be true for ALLOWED."
    );
    requireCondition(
        !result.isRejected(),
        "isRejected() must be false for ALLOWED."
    );
    requireCondition(
        !result.requiresAlert(),
        "requiresAlert() must be false for ALLOWED."
    );
    requireCondition(
        result.depth() == 2,
        "depth() must equal 2."
    );
}

void testDepthBetweenThresholdsIsAllowedWithAlert() {
    ChainReorgGuard guard;

    // depth = 100 - 95 = 5, alert = 3, max = 6
    const auto result = guard.checkReorg(100, 95, kNow);

    requireCondition(
        result.outcome() == ReorgCheckOutcome::ALLOWED_WITH_ALERT,
        "depth=5 should produce ALLOWED_WITH_ALERT outcome."
    );
    requireCondition(
        result.isAllowed(),
        "isAllowed() must be true for ALLOWED_WITH_ALERT."
    );
    requireCondition(
        result.requiresAlert(),
        "requiresAlert() must be true for ALLOWED_WITH_ALERT."
    );
    requireCondition(
        result.depth() == 5,
        "depth() must equal 5."
    );
}

void testDepthExceedingMaxIsRejected() {
    ChainReorgGuard guard;

    // depth = 200 - 190 = 10, max = 6
    const auto result = guard.checkReorg(200, 190, kNow);

    requireCondition(
        result.outcome() == ReorgCheckOutcome::REJECTED,
        "depth=10 should produce REJECTED outcome."
    );
    requireCondition(
        result.isRejected(),
        "isRejected() must be true."
    );
    requireCondition(
        !result.isAllowed(),
        "isAllowed() must be false for REJECTED."
    );
    requireCondition(
        result.depth() == 10,
        "depth() must equal 10."
    );
}

void testExactlyMaxDepthIsRejected() {
    ChainReorgGuard guard;

    // depth = 1006 - 1000 = 6 == maxReorgDepth, which is > allowed range
    // (depth must be <= maxReorgDepth to pass)
    // Actually: depth > maxReorgDepth is rejected, depth == maxReorgDepth is allowed with alert
    const auto result = guard.checkReorg(1006, 1000, kNow);

    // depth = 6 == maxReorgDepth → depth > maxReorgDepth is false → NOT rejected
    // depth = 6 > alertThresholdDepth(3) → ALLOWED_WITH_ALERT
    requireCondition(
        result.outcome() == ReorgCheckOutcome::ALLOWED_WITH_ALERT,
        "depth exactly equal to maxReorgDepth should be ALLOWED_WITH_ALERT (not rejected)."
    );
}

void testDepthOneMoreThanMaxIsRejected() {
    ChainReorgGuard guard;
    // depth = 7 > maxReorgDepth(6)
    const auto result = guard.checkReorg(1007, 1000, kNow);

    requireCondition(
        result.outcome() == ReorgCheckOutcome::REJECTED,
        "depth = maxReorgDepth+1 must be REJECTED."
    );
}

void testHistoryRecording() {
    ChainReorgGuard guard;

    requireCondition(
        guard.reorgHistory().empty(),
        "History must be empty initially."
    );

    const auto r1 = guard.checkReorg(10, 8, kNow);
    const auto r2 = guard.checkReorg(100, 95, kNow);
    const auto r3 = guard.checkReorg(200, 190, kNow);

    const ReorgEvent ev1(kNow, 8, 10, 10, r1.depth(), r1.outcome(), r1.reason());
    const ReorgEvent ev2(kNow, 95, 100, 100, r2.depth(), r2.outcome(), r2.reason());
    const ReorgEvent ev3(kNow, 190, 200, 200, r3.depth(), r3.outcome(), r3.reason());

    guard.recordReorgEvent(ev1);
    guard.recordReorgEvent(ev2);
    guard.recordReorgEvent(ev3);

    requireCondition(
        guard.reorgHistory().size() == 3U,
        "History should contain 3 events."
    );
    requireCondition(
        guard.totalAlertedReorgs() == 1U,
        "1 event should be counted as alerted."
    );
    requireCondition(
        guard.totalRejectedReorgs() == 1U,
        "1 event should be counted as rejected."
    );
}

void testReorgEventIsValid() {
    const ReorgEvent event(kNow, 100, 106, 108, 6,
        ReorgCheckOutcome::ALLOWED_WITH_ALERT, "within bounds");

    requireCondition(
        event.isValid(),
        "Well-formed ReorgEvent should be valid."
    );
    requireCondition(
        event.depth() == 6,
        "depth() must return the recorded depth."
    );
    requireCondition(
        event.fromHeight() == 100,
        "fromHeight() must match."
    );
}

void testDefaultReorgEventIsNotValid() {
    const ReorgEvent empty;
    requireCondition(
        !empty.isValid(),
        "Default-constructed ReorgEvent should not be valid."
    );
}

void testConfigSerialize() {
    const ChainReorgGuardConfig config = ChainReorgGuardConfig::defaults();
    const std::string serialized = config.serialize();

    requireCondition(
        !serialized.empty(),
        "ChainReorgGuardConfig::serialize() must produce a non-empty string."
    );
    requireCondition(
        serialized.find("maxReorgDepth=6") != std::string::npos,
        "Serialized config should contain maxReorgDepth=6."
    );
}

} // namespace

int main() {
    try {
        testConfigDefaultsAreValid();
        testConfigInvalidWhenAlertThresholdExceedsMax();
        testDepthWithinAlertThresholdIsAllowed();
        testDepthBetweenThresholdsIsAllowedWithAlert();
        testDepthExceedingMaxIsRejected();
        testExactlyMaxDepthIsRejected();
        testDepthOneMoreThanMaxIsRejected();
        testHistoryRecording();
        testReorgEventIsValid();
        testDefaultReorgEventIsNotValid();
        testConfigSerialize();

        std::cout << "Nodo ChainReorgGuard tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo ChainReorgGuard tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
