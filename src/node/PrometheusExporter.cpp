#include "node/PrometheusExporter.hpp"

#include <sstream>

namespace nodo::node {
namespace {

std::string labelEscape(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

int healthValue(NodeHealthStatus status) {
  switch (status) {
  case NodeHealthStatus::HEALTHY:
    return 0;
  case NodeHealthStatus::DEGRADED:
    return 1;
  case NodeHealthStatus::UNHEALTHY:
    return 2;
  }
  return 2;
}

std::string labels(const NodeMetricsSnapshot &metrics) {
  return std::string("chain_id=\"") + labelEscape(metrics.chainId) +
         "\",network=\"" + labelEscape(metrics.networkName) + "\",peer_id=\"" +
         labelEscape(metrics.localPeerId) + "\"";
}

} // namespace

std::string
PrometheusExporter::exportMetrics(const NodeMetricsSnapshot &metrics,
                                  NodeHealthStatus healthStatus) {
  const std::string common = labels(metrics);
  std::ostringstream out;
  out << "# HELP nodo_health_status Node health status: 0 healthy, 1 degraded, "
         "2 unhealthy\n"
      << "# TYPE nodo_health_status gauge\n"
      << "nodo_health_status{" << common << ",status=\""
      << labelEscape(nodeHealthStatusToString(healthStatus)) << "\"} "
      << healthValue(healthStatus) << "\n"
      << "# HELP nodo_chain_height Current canonical chain tip height\n"
      << "# TYPE nodo_chain_height gauge\n"
      << "nodo_chain_height{" << common << "} " << metrics.height << "\n"
      << "# HELP nodo_finalized_height Highest finalized block height\n"
      << "# TYPE nodo_finalized_height gauge\n"
      << "nodo_finalized_height{" << common << "} " << metrics.finalizedHeight
      << "\n"
      << "# HELP nodo_finality_lag Chain tip height minus finalized height\n"
      << "# TYPE nodo_finality_lag gauge\n"
      << "nodo_finality_lag{" << common << "} " << metrics.finalityLag << "\n"
      << "# HELP nodo_peer_count Known peer count\n"
      << "# TYPE nodo_peer_count gauge\n"
      << "nodo_peer_count{" << common << "} " << metrics.peerCount << "\n"
      << "# HELP nodo_validator_count Active validator count\n"
      << "# TYPE nodo_validator_count gauge\n"
      << "nodo_validator_count{" << common << "} " << metrics.validatorCount
      << "\n"
      << "# HELP nodo_mempool_size Current mempool transaction count\n"
      << "# TYPE nodo_mempool_size gauge\n"
      << "nodo_mempool_size{" << common << "} " << metrics.mempoolSize << "\n"
      << "# HELP nodo_sync_total_synced Successfully imported sync batches\n"
      << "# TYPE nodo_sync_total_synced counter\n"
      << "nodo_sync_total_synced{" << common << ",sync_status=\""
      << labelEscape(metrics.syncStatus) << "\"} " << metrics.syncTotalSynced
      << "\n"
      << "# HELP nodo_sync_total_failures Total recorded sync failures\n"
      << "# TYPE nodo_sync_total_failures counter\n"
      << "nodo_sync_total_failures{" << common << ",sync_status=\""
      << labelEscape(metrics.syncStatus) << "\"} " << metrics.syncTotalFailures
      << "\n"
      << "# HELP nodo_event_latest_sequence Latest in-memory event sequence\n"
      << "# TYPE nodo_event_latest_sequence gauge\n"
      << "nodo_event_latest_sequence{" << common << "} "
      << metrics.eventLatestSequence << "\n"
      << "# HELP nodo_event_retained_count Retained in-memory event count\n"
      << "# TYPE nodo_event_retained_count gauge\n"
      << "nodo_event_retained_count{" << common << "} "
      << metrics.eventRetainedCount << "\n"
      << "# HELP nodo_rpc_running RPC server running state\n"
      << "# TYPE nodo_rpc_running gauge\n"
      << "nodo_rpc_running{" << common << "} " << (metrics.rpcRunning ? 1 : 0)
      << "\n";
  return out.str();
}

} // namespace nodo::node
