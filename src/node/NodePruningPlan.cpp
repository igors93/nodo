#include "node/NodePruningPlan.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

NodePruningPlan::NodePruningPlan()
    : m_config(NodePruningConfig::archiveMode()), m_currentHeight(0),
      m_retainFromHeight(0), m_snapshotBoundaryHeight(0),
      m_snapshotBoundaryDigest(""), m_blockArtifactsToPrune(),
      m_snapshotsToPrune(), m_safeToApply(false),
      m_reason("Uninitialized pruning plan.") {}

NodePruningPlan::NodePruningPlan(
    NodePruningConfig config, std::uint64_t currentHeight,
    std::uint64_t retainFromHeight, std::uint64_t snapshotBoundaryHeight,
    std::string snapshotBoundaryDigest,
    std::vector<std::filesystem::path> blockArtifactsToPrune,
    std::vector<std::filesystem::path> snapshotsToPrune, bool safeToApply,
    std::string reason)
    : m_config(std::move(config)), m_currentHeight(currentHeight),
      m_retainFromHeight(retainFromHeight),
      m_snapshotBoundaryHeight(snapshotBoundaryHeight),
      m_snapshotBoundaryDigest(std::move(snapshotBoundaryDigest)),
      m_blockArtifactsToPrune(std::move(blockArtifactsToPrune)),
      m_snapshotsToPrune(std::move(snapshotsToPrune)),
      m_safeToApply(safeToApply), m_reason(std::move(reason)) {}

const NodePruningConfig &NodePruningPlan::config() const { return m_config; }
std::uint64_t NodePruningPlan::currentHeight() const { return m_currentHeight; }
std::uint64_t NodePruningPlan::retainFromHeight() const {
  return m_retainFromHeight;
}
std::uint64_t NodePruningPlan::snapshotBoundaryHeight() const {
  return m_snapshotBoundaryHeight;
}
const std::string &NodePruningPlan::snapshotBoundaryDigest() const {
  return m_snapshotBoundaryDigest;
}
const std::vector<std::filesystem::path> &
NodePruningPlan::blockArtifactsToPrune() const {
  return m_blockArtifactsToPrune;
}
const std::vector<std::filesystem::path> &
NodePruningPlan::snapshotsToPrune() const {
  return m_snapshotsToPrune;
}
bool NodePruningPlan::safeToApply() const { return m_safeToApply; }
const std::string &NodePruningPlan::reason() const { return m_reason; }

std::size_t NodePruningPlan::totalFileCount() const {
  return m_blockArtifactsToPrune.size() + m_snapshotsToPrune.size();
}

std::string NodePruningPlan::serialize() const {
  std::ostringstream oss;
  oss << "NodePruningPlan{"
      << "mode=" << nodePruningModeToString(m_config.mode())
      << ";retainEpochs=" << m_config.retainEpochs()
      << ";currentHeight=" << m_currentHeight
      << ";retainFromHeight=" << m_retainFromHeight
      << ";snapshotBoundaryHeight=" << m_snapshotBoundaryHeight
      << ";blockArtifactsToPrune=" << m_blockArtifactsToPrune.size()
      << ";snapshotsToPrune=" << m_snapshotsToPrune.size()
      << ";safeToApply=" << (m_safeToApply ? "true" : "false")
      << ";reason=" << m_reason << "}";
  return oss.str();
}

} // namespace nodo::node
