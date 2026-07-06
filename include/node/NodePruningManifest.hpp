#ifndef NODO_NODE_NODE_PRUNING_MANIFEST_HPP
#define NODO_NODE_NODE_PRUNING_MANIFEST_HPP

#include "node/NodeDataDirectory.hpp"
#include "node/NodePruningConfig.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

/*
 * Durable pruning boundary for a node data directory.
 *
 * The manifest is intentionally small and canonical.  It records what this
 * node is allowed to have removed from disk and the snapshot boundary that
 * makes that removal auditable.  Archive mode is the default and never prunes
 * canonical finalized artifacts.
 */
class NodePruningManifest {
public:
  static constexpr const char *SCHEMA_VERSION = "NODO_PRUNING_MANIFEST_V1";

  NodePruningManifest();

  NodePruningManifest(NodePruningConfig config, std::string chainId,
                      std::string genesisConfigId, std::uint64_t latestHeight,
                      std::string latestBlockHash, std::string latestStateRoot,
                      std::uint64_t retainFromHeight,
                      std::uint64_t snapshotBoundaryHeight,
                      std::string snapshotBoundaryDigest,
                      std::uint64_t prunedBlockArtifactCount,
                      std::uint64_t prunedSnapshotCount,
                      std::int64_t updatedAt);

  const NodePruningConfig &config() const;
  const std::string &chainId() const;
  const std::string &genesisConfigId() const;
  std::uint64_t latestHeight() const;
  const std::string &latestBlockHash() const;
  const std::string &latestStateRoot() const;
  std::uint64_t retainFromHeight() const;
  std::uint64_t snapshotBoundaryHeight() const;
  const std::string &snapshotBoundaryDigest() const;
  std::uint64_t prunedBlockArtifactCount() const;
  std::uint64_t prunedSnapshotCount() const;
  std::int64_t updatedAt() const;

  bool isArchive() const;
  bool isPruned() const;
  bool isValid() const;

  std::string serialize() const;
  std::string toFileContents() const;
  static NodePruningManifest fromFileContents(const std::string &contents);

  static NodePruningManifest archive(const NodeRuntimeManifest &runtimeManifest,
                                     std::int64_t updatedAt);

private:
  NodePruningConfig m_config;
  std::string m_chainId;
  std::string m_genesisConfigId;
  std::uint64_t m_latestHeight;
  std::string m_latestBlockHash;
  std::string m_latestStateRoot;
  std::uint64_t m_retainFromHeight;
  std::uint64_t m_snapshotBoundaryHeight;
  std::string m_snapshotBoundaryDigest;
  std::uint64_t m_prunedBlockArtifactCount;
  std::uint64_t m_prunedSnapshotCount;
  std::int64_t m_updatedAt;
};

} // namespace nodo::node

#endif
