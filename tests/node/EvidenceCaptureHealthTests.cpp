#include "node/EvidenceCaptureHealth.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::EvidenceCaptureHealth;
using nodo::node::EvidenceCaptureStatus;
using nodo::node::evidenceCaptureStatusToString;

// Default-constructed health is DISABLED.
void testDefaultIsDisabled() {
    const EvidenceCaptureHealth health;
    assert(health.status() == EvidenceCaptureStatus::DISABLED);
    assert(health.totalCaptured() == 0);
    assert(health.totalPersistFailures() == 0);
    assert(health.isHealthy()); // DISABLED is acceptable (no store configured)
}

// markUnavailable sets STORE_UNAVAILABLE.
void testMarkUnavailable() {
    EvidenceCaptureHealth health;
    health.markUnavailable();
    assert(health.status() == EvidenceCaptureStatus::STORE_UNAVAILABLE);
    assert(!health.isHealthy());
}

// recordSuccess transitions to HEALTHY and increments counter.
void testRecordSuccess() {
    EvidenceCaptureHealth health;
    health.markUnavailable(); // start unavailable
    health.recordSuccess();
    assert(health.status() == EvidenceCaptureStatus::HEALTHY);
    assert(health.totalCaptured() == 1);
    assert(health.isHealthy());
    health.recordSuccess();
    assert(health.totalCaptured() == 2);
}

// recordFailure transitions to PERSIST_FAILURE.
void testRecordFailure() {
    EvidenceCaptureHealth health;
    health.recordFailure("disk full", 1900000001);
    assert(health.status() == EvidenceCaptureStatus::PERSIST_FAILURE);
    assert(health.totalPersistFailures() == 1);
    assert(!health.isHealthy());
    assert(health.lastFailureAt() == 1900000001);
    assert(health.lastFailureReason() == "disk full");
}

// hasRecentFailure returns true within window.
void testHasRecentFailure() {
    EvidenceCaptureHealth health;
    health.recordFailure("reason", 1000);
    assert(health.hasRecentFailure(1059, 60));
    assert(!health.hasRecentFailure(1061, 60));
}

// Multiple failures accumulate.
void testMultipleFailures() {
    EvidenceCaptureHealth health;
    health.recordFailure("err1", 100);
    health.recordFailure("err2", 200);
    assert(health.totalPersistFailures() == 2);
    assert(health.lastFailureReason() == "err2");
}

// Status to string.
void testStatusToString() {
    assert(evidenceCaptureStatusToString(EvidenceCaptureStatus::HEALTHY) == "HEALTHY");
    assert(evidenceCaptureStatusToString(EvidenceCaptureStatus::STORE_UNAVAILABLE) == "STORE_UNAVAILABLE");
    assert(evidenceCaptureStatusToString(EvidenceCaptureStatus::PERSIST_FAILURE) == "PERSIST_FAILURE");
    assert(evidenceCaptureStatusToString(EvidenceCaptureStatus::DISABLED) == "DISABLED");
}

// serialize output is non-empty.
void testSerializeNonEmpty() {
    EvidenceCaptureHealth health;
    health.recordSuccess();
    const auto s = health.serialize();
    assert(!s.empty());
    assert(s.find("HEALTHY") != std::string::npos);
}

} // namespace

int main() {
    testDefaultIsDisabled();
    testMarkUnavailable();
    testRecordSuccess();
    testRecordFailure();
    testHasRecentFailure();
    testMultipleFailures();
    testStatusToString();
    testSerializeNonEmpty();
    return 0;
}
