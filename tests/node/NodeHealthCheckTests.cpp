#include "node/HealthCheckService.hpp"
#include "node/PrometheusExporter.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using nodo::node::HealthCheckService;
using nodo::node::NodeHealthPolicy;
using nodo::node::NodeHealthStatus;
using nodo::node::nodeHealthStatusToString;
using nodo::node::NodeMetricsSnapshot;
using nodo::node::PrometheusExporter;

namespace {

NodeMetricsSnapshot baseMetrics() {
  NodeMetricsSnapshot metrics;
  metrics.collectedAt = 1000;
  metrics.networkName = "localnet";
  metrics.chainId = "nodo-localnet-1";
  metrics.genesisConfigId = "genesis-id";
  metrics.localPeerId = "node-a";
  metrics.runtimeStatus = "RUNNING";
  metrics.height = 10;
  metrics.finalizedHeight = 10;
  metrics.finalityLag = 0;
  metrics.latestHash = "hash10";
  metrics.latestStateRoot = "state10";
  metrics.peerCount = 2;
  metrics.validatorCount = 3;
  metrics.mempoolSize = 1;
  metrics.eventLatestSequence = 5;
  metrics.eventRetainedCount = 5;
  metrics.eventMaxRetainedCount = 4096;
  metrics.syncStatus = "HEALTHY";
  metrics.syncHealthy = true;
  metrics.rpcRunning = true;
  metrics.runtimeValid = true;
  metrics.runtimeRunning = true;
  metrics.runtimeHalted = false;
  return metrics;
}

void testHealthyMetricsPass() {
  const auto report = HealthCheckService::evaluate(baseMetrics());
  assert(report.status() == NodeHealthStatus::HEALTHY);
  assert(report.healthy());
  assert(report.serializeJson().find("\"status\":\"HEALTHY\"") !=
         std::string::npos);
}

void testRecentSyncFailureDegrades() {
  auto metrics = baseMetrics();
  metrics.syncHealthy = false;
  metrics.syncStatus = "BATCH_FAILURE";
  metrics.syncLastFailureAt = 980;
  metrics.syncLastFailureReason = "bad batch";

  const auto report = HealthCheckService::evaluate(metrics);
  assert(report.status() == NodeHealthStatus::DEGRADED);
  assert(!report.healthy());
  assert(report.serializeJson().find("bad batch") != std::string::npos);
}

void testRuntimeHaltedIsUnhealthy() {
  auto metrics = baseMetrics();
  metrics.runtimeHalted = true;
  metrics.runtimeStatus = "HALTED";

  const auto report = HealthCheckService::evaluate(metrics);
  assert(report.status() == NodeHealthStatus::UNHEALTHY);
}

void testFinalityLagThresholds() {
  auto metrics = baseMetrics();
  metrics.height = 100;
  metrics.finalizedHeight = 94;
  metrics.finalityLag = 6;

  NodeHealthPolicy policy;
  policy.degradedFinalityLag = 5;
  policy.unhealthyFinalityLag = 50;
  assert(HealthCheckService::evaluate(metrics, policy).status() ==
         NodeHealthStatus::DEGRADED);

  metrics.finalizedHeight = 25;
  metrics.finalityLag = 75;
  assert(HealthCheckService::evaluate(metrics, policy).status() ==
         NodeHealthStatus::UNHEALTHY);
}

void testPrometheusExporterContainsCoreMetrics() {
  const auto metrics = baseMetrics();
  const std::string out =
      PrometheusExporter::exportMetrics(metrics, NodeHealthStatus::HEALTHY);
  assert(out.find("nodo_chain_height") != std::string::npos);
  assert(out.find("nodo_finalized_height") != std::string::npos);
  assert(out.find("nodo_sync_total_failures") != std::string::npos);
  assert(out.find("chain_id=\"nodo-localnet-1\"") != std::string::npos);
}

void testStatusStrings() {
  assert(nodeHealthStatusToString(NodeHealthStatus::HEALTHY) == "HEALTHY");
  assert(nodeHealthStatusToString(NodeHealthStatus::DEGRADED) == "DEGRADED");
  assert(nodeHealthStatusToString(NodeHealthStatus::UNHEALTHY) == "UNHEALTHY");
}

} // namespace

int main() {
  try {
    testHealthyMetricsPass();
    testRecentSyncFailureDegrades();
    testRuntimeHaltedIsUnhealthy();
    testFinalityLagThresholds();
    testPrometheusExporterContainsCoreMetrics();
    testStatusStrings();
    std::cout << "Nodo health check tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Nodo health check tests failed: " << e.what() << "\n";
    return 1;
  }
}
