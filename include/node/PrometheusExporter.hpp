#ifndef NODO_NODE_PROMETHEUS_EXPORTER_HPP
#define NODO_NODE_PROMETHEUS_EXPORTER_HPP

#include "node/HealthCheckService.hpp"
#include "node/NodeMetrics.hpp"

#include <string>

namespace nodo::node {

class PrometheusExporter {
public:
  static std::string exportMetrics(const NodeMetricsSnapshot &metrics,
                                   NodeHealthStatus healthStatus);
};

} // namespace nodo::node

#endif
