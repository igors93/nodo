#include "node/SyncHealth.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::SyncHealth;
using nodo::node::SyncHealthStatus;
using nodo::node::syncHealthStatusToString;

void testDefaultIsDisabled() {
  const SyncHealth h;
  assert(h.status() == SyncHealthStatus::DISABLED);
  assert(h.totalSynced() == 0);
  assert(h.totalFailures() == 0);
  assert(h.isHealthy());
}

void testRecordSuccess() {
  SyncHealth h;
  h.recordSuccess(0);
  assert(h.status() == SyncHealthStatus::HEALTHY);
  assert(h.totalSynced() == 1);
  assert(h.isHealthy());
  h.recordSuccess(0);
  assert(h.totalSynced() == 2);
}

void testRecordBatchFailure() {
  SyncHealth h;
  h.recordBatchFailure("bad QC", 1000);
  assert(h.status() == SyncHealthStatus::BATCH_FAILURE);
  assert(h.totalFailures() == 1);
  assert(!h.isHealthy());
  assert(h.lastFailureAt() == 1000);
  assert(h.lastFailureReason() == "bad QC");
}

void testRecordRequestFailure() {
  SyncHealth h;
  h.recordRequestFailure("send error", 2000);
  assert(h.status() == SyncHealthStatus::REQUEST_FAILURE);
  assert(h.totalFailures() == 1);
  assert(!h.isHealthy());
}

void testRecordServeFailure() {
  SyncHealth h;
  h.recordServeFailure("build failed", 3000);
  assert(h.status() == SyncHealthStatus::SERVE_FAILURE);
  assert(h.totalFailures() == 1);
  assert(!h.isHealthy());
}

void testSuccessResetsToHealthy() {
  SyncHealth h;
  h.recordBatchFailure("err", 100);
  assert(!h.isHealthy());
  h.recordSuccess(0);
  assert(h.status() == SyncHealthStatus::HEALTHY);
  assert(h.isHealthy());
  assert(h.totalFailures() == 1);
  assert(h.totalSynced() == 1);
}

void testHasRecentFailure() {
  SyncHealth h;
  h.recordBatchFailure("reason", 1000);
  assert(h.hasRecentFailure(1059, 60));
  assert(!h.hasRecentFailure(1061, 60));
}

void testMultipleFailuresAccumulate() {
  SyncHealth h;
  h.recordBatchFailure("err1", 100);
  h.recordServeFailure("err2", 200);
  assert(h.totalFailures() == 2);
  assert(h.lastFailureReason() == "err2");
  assert(h.lastFailureAt() == 200);
}

void testStatusToString() {
  assert(syncHealthStatusToString(SyncHealthStatus::HEALTHY) == "HEALTHY");
  assert(syncHealthStatusToString(SyncHealthStatus::BATCH_FAILURE) ==
         "BATCH_FAILURE");
  assert(syncHealthStatusToString(SyncHealthStatus::REQUEST_FAILURE) ==
         "REQUEST_FAILURE");
  assert(syncHealthStatusToString(SyncHealthStatus::SERVE_FAILURE) ==
         "SERVE_FAILURE");
  assert(syncHealthStatusToString(SyncHealthStatus::DISABLED) == "DISABLED");
}

void testSerializeIsNonEmpty() {
  SyncHealth h;
  h.recordSuccess(0);
  const auto s = h.serialize();
  assert(!s.empty());
  assert(s.find("HEALTHY") != std::string::npos);
  assert(s.find("totalSynced=1") != std::string::npos);
}

void testLongReasonIsTruncated() {
  SyncHealth h;
  const std::string longReason(512, 'x');
  h.recordBatchFailure(longReason, 1);
  assert(h.lastFailureReason().size() == 256);
}

} // namespace

int main() {
  testDefaultIsDisabled();
  testRecordSuccess();
  testRecordBatchFailure();
  testRecordRequestFailure();
  testRecordServeFailure();
  testSuccessResetsToHealthy();
  testHasRecentFailure();
  testMultipleFailuresAccumulate();
  testStatusToString();
  testSerializeIsNonEmpty();
  testLongReasonIsTruncated();
  return 0;
}
