#ifndef NODO_NODE_HEALTH_CHECK_SERVICE_HPP
#define NODO_NODE_HEALTH_CHECK_SERVICE_HPP

#include "node/NodeMetrics.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

enum class NodeHealthStatus { HEALTHY, DEGRADED, UNHEALTHY };

std::string nodeHealthStatusToString(NodeHealthStatus status);

class NodeHealthPolicy {
public:
  std::uint64_t degradedFinalityLag{5};
  std::uint64_t unhealthyFinalityLag{50};
  std::int64_t recentSyncFailureWindowSeconds{300};

  bool isValid() const;
};

class NodeHealthReport {
public:
  NodeHealthReport();
  NodeHealthReport(NodeHealthStatus status, std::vector<std::string> reasons,
                   NodeMetricsSnapshot metrics);

  NodeHealthStatus status() const;
  const std::vector<std::string> &reasons() const;
  const NodeMetricsSnapshot &metrics() const;

  bool healthy() const;
  std::string serializeJson() const;

private:
  NodeHealthStatus m_status;
  std::vector<std::string> m_reasons;
  NodeMetricsSnapshot m_metrics;
};

class HealthCheckService {
public:
  static NodeHealthReport
  evaluate(const NodeMetricsSnapshot &metrics,
           NodeHealthPolicy policy = NodeHealthPolicy());
};

} // namespace nodo::node

#endif
