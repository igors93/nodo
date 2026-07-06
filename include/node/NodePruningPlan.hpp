#ifndef NODO_NODE_NODE_PRUNING_PLAN_HPP
#define NODO_NODE_NODE_PRUNING_PLAN_HPP

#include "node/NodePruningConfig.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodo::node {

class NodePruningPlan {
public:
  NodePruningPlan();

  NodePruningPlan(NodePruningConfig config, std::uint64_t currentHeight,
                  std::uint64_t retainFromHeight,
                  std::uint64_t snapshotBoundaryHeight,
                  std::string snapshotBoundaryDigest,
                  std::vector<std::filesystem::path> blockArtifactsToPrune,
                  std::vector<std::filesystem::path> snapshotsToPrune,
                  bool safeToApply, std::string reason);

  const NodePruningConfig &config() const;
  std::uint64_t currentHeight() const;
  std::uint64_t retainFromHeight() const;
  std::uint64_t snapshotBoundaryHeight() const;
  const std::string &snapshotBoundaryDigest() const;
  const std::vector<std::filesystem::path> &blockArtifactsToPrune() const;
  const std::vector<std::filesystem::path> &snapshotsToPrune() const;
  bool safeToApply() const;
  const std::string &reason() const;

  std::size_t totalFileCount() const;
  std::string serialize() const;

private:
  NodePruningConfig m_config;
  std::uint64_t m_currentHeight;
  std::uint64_t m_retainFromHeight;
  std::uint64_t m_snapshotBoundaryHeight;
  std::string m_snapshotBoundaryDigest;
  std::vector<std::filesystem::path> m_blockArtifactsToPrune;
  std::vector<std::filesystem::path> m_snapshotsToPrune;
  bool m_safeToApply;
  std::string m_reason;
};

} // namespace nodo::node

#endif
