#ifndef NODO_NODE_NODE_PRUNING_SERVICE_HPP
#define NODO_NODE_NODE_PRUNING_SERVICE_HPP

#include "node/NodeDataDirectory.hpp"
#include "node/NodePruningConfig.hpp"
#include "node/NodePruningManifest.hpp"
#include "node/NodePruningPlan.hpp"

#include <optional>
#include <string>

namespace nodo::node {

enum class NodePruningStatus { APPLIED, NOOP, REJECTED };

std::string nodePruningStatusToString(NodePruningStatus status);

class NodePruningResult {
public:
  NodePruningResult();

  static NodePruningResult applied(NodePruningManifest manifest,
                                   NodePruningPlan plan, std::string message);

  static NodePruningResult noop(NodePruningManifest manifest,
                                NodePruningPlan plan, std::string message);

  static NodePruningResult rejected(std::string reason);

  NodePruningStatus status() const;
  const std::string &reason() const;
  bool success() const;
  bool applied() const;
  const std::optional<NodePruningManifest> &manifest() const;
  const std::optional<NodePruningPlan> &plan() const;
  std::string serialize() const;

private:
  NodePruningStatus m_status;
  std::string m_reason;
  std::optional<NodePruningManifest> m_manifest;
  std::optional<NodePruningPlan> m_plan;
};

class NodePruningService {
public:
  static std::optional<NodePruningManifest>
  loadManifest(const NodeDataDirectoryConfig &directoryConfig);

  static NodePruningPlan
  buildPlan(const NodeDataDirectoryConfig &directoryConfig,
            const NodeRuntimeManifest &runtimeManifest,
            const NodePruningConfig &pruningConfig);

  static NodePruningResult apply(const NodeDataDirectoryConfig &directoryConfig,
                                 const NodeRuntimeManifest &runtimeManifest,
                                 const NodePruningConfig &pruningConfig,
                                 std::int64_t now);

  /*
   * Called after block persistence.  If no pruning manifest exists, archive
   * mode is assumed and no files are touched.  If a non-archive manifest
   * exists, the same policy is reapplied against the updated chain tip.
   */
  static NodePruningResult
  applyConfiguredPolicy(const NodeDataDirectoryConfig &directoryConfig,
                        const NodeRuntimeManifest &runtimeManifest,
                        std::int64_t now);

  static bool
  validateManifestAgainstRuntime(const NodeDataDirectoryConfig &directoryConfig,
                                 const NodeRuntimeManifest &runtimeManifest,
                                 std::string &reason);
};

} // namespace nodo::node

#endif
