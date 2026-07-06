#include "node/NodePruningManifest.hpp"

#include "serialization/KeyValueFileCodec.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string &value) {
  if (value.empty())
    return false;
  for (char c : value) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                    c == '.' || c == ':' || c == '/' || c == '#';
    if (!ok)
      return false;
  }
  return true;
}

bool isHashLike(const std::string &value) {
  if (value.size() != 64)
    return false;
  bool nonZero = false;
  for (char c : value) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!ok)
      return false;
    if (c != '0')
      nonZero = true;
  }
  return nonZero;
}

std::uint64_t parseU64(const serialization::KeyValueFileDocument &doc,
                       const std::string &key) {
  const std::string value = doc.requireField(key);
  if (value.empty()) {
    throw std::invalid_argument("empty pruning manifest numeric field: " + key);
  }
  for (char c : value) {
    if (c < '0' || c > '9') {
      throw std::invalid_argument("malformed pruning manifest numeric field: " +
                                  key);
    }
  }
  std::size_t used = 0;
  const unsigned long long parsed = std::stoull(value, &used);
  if (used != value.size() || std::to_string(parsed) != value) {
    throw std::invalid_argument(
        "non-canonical pruning manifest numeric field: " + key);
  }
  return static_cast<std::uint64_t>(parsed);
}

std::int64_t parseI64(const serialization::KeyValueFileDocument &doc,
                      const std::string &key) {
  const std::string value = doc.requireField(key);
  if (value.empty()) {
    throw std::invalid_argument("empty pruning manifest timestamp field: " +
                                key);
  }
  std::size_t start = 0;
  if (value[0] == '-')
    start = 1;
  if (start >= value.size()) {
    throw std::invalid_argument("malformed pruning manifest timestamp field: " +
                                key);
  }
  for (std::size_t i = start; i < value.size(); ++i) {
    if (value[i] < '0' || value[i] > '9') {
      throw std::invalid_argument(
          "malformed pruning manifest timestamp field: " + key);
    }
  }
  std::size_t used = 0;
  const long long parsed = std::stoll(value, &used);
  if (used != value.size()) {
    throw std::invalid_argument(
        "non-canonical pruning manifest timestamp field: " + key);
  }
  return static_cast<std::int64_t>(parsed);
}

NodePruningMode parseMode(const std::string &value) {
  if (value == "ARCHIVE")
    return NodePruningMode::ARCHIVE;
  if (value == "FULL")
    return NodePruningMode::FULL;
  if (value == "LIGHT")
    return NodePruningMode::LIGHT;
  throw std::invalid_argument("unknown pruning mode: " + value);
}

} // namespace

NodePruningManifest::NodePruningManifest()
    : m_config(NodePruningConfig::archiveMode()), m_chainId(""),
      m_genesisConfigId(""), m_latestHeight(0), m_latestBlockHash(""),
      m_latestStateRoot(""), m_retainFromHeight(0), m_snapshotBoundaryHeight(0),
      m_snapshotBoundaryDigest("NONE"), m_prunedBlockArtifactCount(0),
      m_prunedSnapshotCount(0), m_updatedAt(0) {}

NodePruningManifest::NodePruningManifest(
    NodePruningConfig config, std::string chainId, std::string genesisConfigId,
    std::uint64_t latestHeight, std::string latestBlockHash,
    std::string latestStateRoot, std::uint64_t retainFromHeight,
    std::uint64_t snapshotBoundaryHeight, std::string snapshotBoundaryDigest,
    std::uint64_t prunedBlockArtifactCount, std::uint64_t prunedSnapshotCount,
    std::int64_t updatedAt)
    : m_config(std::move(config)), m_chainId(std::move(chainId)),
      m_genesisConfigId(std::move(genesisConfigId)),
      m_latestHeight(latestHeight),
      m_latestBlockHash(std::move(latestBlockHash)),
      m_latestStateRoot(std::move(latestStateRoot)),
      m_retainFromHeight(retainFromHeight),
      m_snapshotBoundaryHeight(snapshotBoundaryHeight),
      m_snapshotBoundaryDigest(std::move(snapshotBoundaryDigest)),
      m_prunedBlockArtifactCount(prunedBlockArtifactCount),
      m_prunedSnapshotCount(prunedSnapshotCount), m_updatedAt(updatedAt) {}

const NodePruningConfig &NodePruningManifest::config() const {
  return m_config;
}
const std::string &NodePruningManifest::chainId() const { return m_chainId; }
const std::string &NodePruningManifest::genesisConfigId() const {
  return m_genesisConfigId;
}
std::uint64_t NodePruningManifest::latestHeight() const {
  return m_latestHeight;
}
const std::string &NodePruningManifest::latestBlockHash() const {
  return m_latestBlockHash;
}
const std::string &NodePruningManifest::latestStateRoot() const {
  return m_latestStateRoot;
}
std::uint64_t NodePruningManifest::retainFromHeight() const {
  return m_retainFromHeight;
}
std::uint64_t NodePruningManifest::snapshotBoundaryHeight() const {
  return m_snapshotBoundaryHeight;
}
const std::string &NodePruningManifest::snapshotBoundaryDigest() const {
  return m_snapshotBoundaryDigest;
}
std::uint64_t NodePruningManifest::prunedBlockArtifactCount() const {
  return m_prunedBlockArtifactCount;
}
std::uint64_t NodePruningManifest::prunedSnapshotCount() const {
  return m_prunedSnapshotCount;
}
std::int64_t NodePruningManifest::updatedAt() const { return m_updatedAt; }

bool NodePruningManifest::isArchive() const {
  return m_config.mode() == NodePruningMode::ARCHIVE;
}

bool NodePruningManifest::isPruned() const {
  return m_prunedBlockArtifactCount > 0 || m_prunedSnapshotCount > 0 ||
         m_config.mode() != NodePruningMode::ARCHIVE;
}

bool NodePruningManifest::isValid() const {
  if (!m_config.isValid() || !isSafeScalar(m_chainId) ||
      !isHashLike(m_genesisConfigId) || !isHashLike(m_latestBlockHash) ||
      !isHashLike(m_latestStateRoot) || m_updatedAt <= 0) {
    return false;
  }

  if (m_retainFromHeight > m_latestHeight)
    return false;

  if (m_config.mode() == NodePruningMode::ARCHIVE) {
    return m_retainFromHeight == 0 && m_snapshotBoundaryHeight == 0 &&
           m_snapshotBoundaryDigest == "NONE" &&
           m_prunedBlockArtifactCount == 0;
  }

  if (m_snapshotBoundaryHeight == 0 ||
      m_snapshotBoundaryHeight < m_retainFromHeight ||
      m_snapshotBoundaryHeight > m_latestHeight ||
      m_snapshotBoundaryDigest.empty()) {
    return false;
  }

  return true;
}

std::string NodePruningManifest::serialize() const {
  std::ostringstream oss;
  oss << "NodePruningManifest{"
      << "mode=" << nodePruningModeToString(m_config.mode())
      << ";retainEpochs=" << m_config.retainEpochs() << ";chainId=" << m_chainId
      << ";genesisConfigId=" << m_genesisConfigId
      << ";latestHeight=" << m_latestHeight
      << ";latestBlockHash=" << m_latestBlockHash
      << ";latestStateRoot=" << m_latestStateRoot
      << ";retainFromHeight=" << m_retainFromHeight
      << ";snapshotBoundaryHeight=" << m_snapshotBoundaryHeight
      << ";snapshotBoundaryDigest=" << m_snapshotBoundaryDigest
      << ";prunedBlockArtifactCount=" << m_prunedBlockArtifactCount
      << ";prunedSnapshotCount=" << m_prunedSnapshotCount
      << ";updatedAt=" << m_updatedAt << "}";
  return oss.str();
}

std::string NodePruningManifest::toFileContents() const {
  return serialization::KeyValueFileCodec::serialize(
      SCHEMA_VERSION,
      {{"mode", nodePruningModeToString(m_config.mode())},
       {"retainEpochs", std::to_string(m_config.retainEpochs())},
       {"chainId", m_chainId},
       {"genesisConfigId", m_genesisConfigId},
       {"latestHeight", std::to_string(m_latestHeight)},
       {"latestBlockHash", m_latestBlockHash},
       {"latestStateRoot", m_latestStateRoot},
       {"retainFromHeight", std::to_string(m_retainFromHeight)},
       {"snapshotBoundaryHeight", std::to_string(m_snapshotBoundaryHeight)},
       {"snapshotBoundaryDigest", m_snapshotBoundaryDigest},
       {"prunedBlockArtifactCount", std::to_string(m_prunedBlockArtifactCount)},
       {"prunedSnapshotCount", std::to_string(m_prunedSnapshotCount)},
       {"updatedAt", std::to_string(m_updatedAt)}});
}

NodePruningManifest
NodePruningManifest::fromFileContents(const std::string &contents) {
  const serialization::KeyValueFileDocument doc =
      serialization::KeyValueFileCodec::parse(contents, SCHEMA_VERSION);

  doc.requireOnlyFields({"mode", "retainEpochs", "chainId", "genesisConfigId",
                         "latestHeight", "latestBlockHash", "latestStateRoot",
                         "retainFromHeight", "snapshotBoundaryHeight",
                         "snapshotBoundaryDigest", "prunedBlockArtifactCount",
                         "prunedSnapshotCount", "updatedAt"});

  const NodePruningMode mode = parseMode(doc.requireField("mode"));
  const std::uint64_t retainEpochs = parseU64(doc, "retainEpochs");
  NodePruningConfig config = NodePruningConfig::archiveMode();
  if (mode == NodePruningMode::FULL) {
    config =
        NodePruningConfig::fullMode(static_cast<std::size_t>(retainEpochs));
  } else if (mode == NodePruningMode::LIGHT) {
    config = NodePruningConfig::lightMode();
  }

  NodePruningManifest manifest(
      config, doc.requireField("chainId"), doc.requireField("genesisConfigId"),
      parseU64(doc, "latestHeight"), doc.requireField("latestBlockHash"),
      doc.requireField("latestStateRoot"), parseU64(doc, "retainFromHeight"),
      parseU64(doc, "snapshotBoundaryHeight"),
      doc.requireField("snapshotBoundaryDigest"),
      parseU64(doc, "prunedBlockArtifactCount"),
      parseU64(doc, "prunedSnapshotCount"), parseI64(doc, "updatedAt"));

  if (!manifest.isValid()) {
    throw std::invalid_argument("parsed pruning manifest is invalid");
  }

  if (contents != manifest.toFileContents()) {
    throw std::invalid_argument("pruning manifest is not canonical");
  }

  return manifest;
}

NodePruningManifest
NodePruningManifest::archive(const NodeRuntimeManifest &runtimeManifest,
                             std::int64_t updatedAt) {
  return NodePruningManifest(
      NodePruningConfig::archiveMode(), runtimeManifest.chainId(),
      runtimeManifest.genesisConfigId(), runtimeManifest.latestBlockHeight(),
      runtimeManifest.latestBlockHash(), runtimeManifest.latestStateRoot(), 0,
      0, "NONE", 0, 0, updatedAt);
}

} // namespace nodo::node
