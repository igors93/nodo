#ifndef NODO_NODE_NODE_PRUNING_SERVICE_HPP
#define NODO_NODE_NODE_PRUNING_SERVICE_HPP

#include "node/NodeDataDirectory.hpp"
#include "node/NodePruningConfig.hpp"
#include "node/NodePruningManifest.hpp"
#include "node/NodePruningPlan.hpp"
#include "node/history/HistoryPruningEngine.hpp"

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

  static NodePruningResult fromRun(const PruningRunResult &run);
  // Set when HistoryPruningEngine handled the request.
  const std::optional<PruningRunResult> &run() const;

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
  std::optional<PruningRunResult> m_run;
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
   * Called after block persistence. Archive mode (the default, also when no
   * manifest exists) touches nothing. A v2 NORMAL/LIGHT policy runs the
   * crash-safe HistoryPruningEngine once per newly confirmed checkpoint. A
   * legacy v1 manifest is never re-applied, because its LIGHT mode deleted
   * finalized block files that reload still needs.
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
