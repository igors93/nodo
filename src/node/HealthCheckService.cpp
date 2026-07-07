#include "node/HealthCheckService.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {
namespace {

std::string jsonString(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
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
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

} // namespace

std::string nodeHealthStatusToString(NodeHealthStatus status) {
  switch (status) {
  case NodeHealthStatus::HEALTHY:
    return "HEALTHY";
  case NodeHealthStatus::DEGRADED:
    return "DEGRADED";
  case NodeHealthStatus::UNHEALTHY:
    return "UNHEALTHY";
  }
  return "UNKNOWN";
}

bool NodeHealthPolicy::isValid() const {
  return degradedFinalityLag <= unhealthyFinalityLag &&
         recentSyncFailureWindowSeconds >= 0;
}

NodeHealthReport::NodeHealthReport()
    : m_status(NodeHealthStatus::UNHEALTHY),
      m_reasons({"health report not evaluated"}), m_metrics() {}

NodeHealthReport::NodeHealthReport(NodeHealthStatus status,
                                   std::vector<std::string> reasons,
                                   NodeMetricsSnapshot metrics)
    : m_status(status), m_reasons(std::move(reasons)),
      m_metrics(std::move(metrics)) {}

NodeHealthStatus NodeHealthReport::status() const { return m_status; }
const std::vector<std::string> &NodeHealthReport::reasons() const {
  return m_reasons;
}
const NodeMetricsSnapshot &NodeHealthReport::metrics() const {
  return m_metrics;
}
bool NodeHealthReport::healthy() const {
  return m_status == NodeHealthStatus::HEALTHY;
}

std::string NodeHealthReport::serializeJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"status\":" << jsonString(nodeHealthStatusToString(m_status))
      << ",\"healthy\":" << (healthy() ? "true" : "false") << ",\"reasons\":[";
  for (std::size_t i = 0; i < m_reasons.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << jsonString(m_reasons[i]);
  }
  oss << "]"
      << ",\"metrics\":" << m_metrics.serializeJson() << "}";
  return oss.str();
}

NodeHealthReport
HealthCheckService::evaluate(const NodeMetricsSnapshot &metrics,
                             NodeHealthPolicy policy) {
  std::vector<std::string> reasons;
  NodeHealthStatus status = NodeHealthStatus::HEALTHY;

  if (!policy.isValid()) {
    return NodeHealthReport(NodeHealthStatus::UNHEALTHY,
                            {"invalid health policy"}, metrics);
  }

  auto degrade = [&](const std::string &reason) {
    reasons.push_back(reason);
    if (status == NodeHealthStatus::HEALTHY) {
      status = NodeHealthStatus::DEGRADED;
    }
  };
  auto unhealthy = [&](const std::string &reason) {
    reasons.push_back(reason);
    status = NodeHealthStatus::UNHEALTHY;
  };

  if (!metrics.runtimeValid) {
    unhealthy("runtime state is invalid");
  }
  if (metrics.runtimeHalted || metrics.runtimeStatus == "HALTED") {
    unhealthy("runtime is halted");
  }
  if (!metrics.rpcRunning) {
    degrade(metrics.rpcStartError.empty()
                ? "RPC server is not running"
                : "RPC server is not running: " + metrics.rpcStartError);
  }
  if (!metrics.syncHealthy) {
    degrade(metrics.syncLastFailureReason.empty()
                ? "sync pipeline has recorded failures"
                : "sync pipeline failure: " + metrics.syncLastFailureReason);
  }
  if (metrics.syncLastFailureAt > 0 &&
      metrics.collectedAt >= metrics.syncLastFailureAt &&
      metrics.collectedAt - metrics.syncLastFailureAt <=
          policy.recentSyncFailureWindowSeconds) {
    degrade("recent sync failure observed");
  }
  if (metrics.finalityLag >= policy.unhealthyFinalityLag) {
    unhealthy("finality lag exceeds unhealthy threshold");
  } else if (metrics.finalityLag >= policy.degradedFinalityLag) {
    degrade("finality lag exceeds degraded threshold");
  }

  if (reasons.empty()) {
    reasons.push_back("node is healthy");
  }

  return NodeHealthReport(status, reasons, metrics);
}

} // namespace nodo::node
