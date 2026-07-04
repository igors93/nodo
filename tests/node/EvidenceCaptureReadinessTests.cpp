#include "config/NetworkParameters.hpp"
#include "crypto/KeyStore.hpp"
#include "node/EvidenceCaptureHealth.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/ReadinessContext.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::EvidenceCaptureHealth;
using nodo::node::EvidenceCaptureStatus;
using nodo::node::ReadinessContextBuilder;

// Minimal test infrastructure — we only need to exercise the
// withEvidenceCaptureHealth() path.

nodo::config::NetworkParameters localnetParams() {
  return nodo::config::NetworkParameters::developmentLocal();
}

nodo::node::NodeDataDirectoryConfig testDirConfig() {
  return nodo::node::NodeDataDirectoryConfig(".nodo-test-tmp");
}

nodo::crypto::StoredKeyMetadata emptyKeyMetadata() {
  return nodo::crypto::StoredKeyMetadata();
}

// Test 13: Evidence capture success updates health to HEALTHY.
void testCaptureSuccessUpdatesHealth() {
  EvidenceCaptureHealth health;
  health.recordSuccess();
  assert(health.status() == EvidenceCaptureStatus::HEALTHY);
  assert(health.totalCaptured() == 1);
  assert(health.isHealthy());
}

// Test 14: Evidence capture failure updates diagnostics.
void testCaptureFailureUpdatesDiagnostics() {
  EvidenceCaptureHealth health;
  health.recordFailure("disk full", 1900000001);
  assert(health.status() == EvidenceCaptureStatus::PERSIST_FAILURE);
  assert(!health.isHealthy());
  assert(health.totalPersistFailures() == 1);
  assert(health.lastFailureReason() == "disk full");
  assert(health.lastFailureAt() == 1900000001);
}

// Test 15: Readiness adds warning when evidence capture is unhealthy.
void testReadinessWarnsWhenCaptureUnhealthy() {
  EvidenceCaptureHealth health;
  health.markUnavailable();
  assert(!health.isHealthy());

  ReadinessContextBuilder builder(testDirConfig(), localnetParams(),
                                  emptyKeyMetadata());
  builder.withEvidenceCaptureHealth(health);

  const auto ctx = builder.build();
  assert(!ctx.evidenceCaptureHealthy);
  // A warning should have been added.
  bool foundWarning = false;
  for (const auto &w : ctx.warnings) {
    if (w.find("evidence") != std::string::npos) {
      foundWarning = true;
    }
  }
  assert(foundWarning);
}

// Intentionally DISABLED capture does not fail readiness.
void testDisabledCaptureDoesNotFailReadiness() {
  EvidenceCaptureHealth health; // default is DISABLED
  assert(health.status() == EvidenceCaptureStatus::DISABLED);

  ReadinessContextBuilder builder(testDirConfig(), localnetParams(),
                                  emptyKeyMetadata());
  builder.withEvidenceCaptureHealth(health);

  const auto ctx = builder.build();
  assert(ctx.evidenceCaptureHealthy); // DISABLED is acceptable
  assert(ctx.evidenceCaptureDisabled);

  // No evidence-related warning should be present.
  for (const auto &w : ctx.warnings) {
    assert(w.find("evidence") == std::string::npos);
  }
}

// HEALTHY capture keeps readiness healthy.
void testHealthyCaptureKeepsReadinessHealthy() {
  EvidenceCaptureHealth health;
  health.recordSuccess();

  ReadinessContextBuilder builder(testDirConfig(), localnetParams(),
                                  emptyKeyMetadata());
  builder.withEvidenceCaptureHealth(health);

  const auto ctx = builder.build();
  assert(ctx.evidenceCaptureHealthy);
  assert(!ctx.evidenceCaptureDisabled);
}

} // namespace

int main() {
  testCaptureSuccessUpdatesHealth();
  testCaptureFailureUpdatesDiagnostics();
  testReadinessWarnsWhenCaptureUnhealthy();
  testDisabledCaptureDoesNotFailReadiness();
  testHealthyCaptureKeepsReadinessHealthy();
  return 0;
}
