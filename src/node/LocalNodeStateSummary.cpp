#include "node/LocalNodeStateSummary.hpp"

#include <sstream>

namespace nodo::node {

LocalNodeStateSummary::LocalNodeStateSummary()
    : m_nodeId(),
      m_endpoint(),
      m_dataDirectory(),
      m_networkName(),
      m_genesisId(),
      m_latestHeight(0),
      m_latestBlockHash(),
      m_latestStateRoot(),
      m_readable(false),
      m_readError("not initialized") {}

LocalNodeStateSummary LocalNodeStateSummary::fromDataDirectory(
    const NodeDataDirectoryConfig& config,
    const std::string& nodeId,
    const std::string& endpoint
) {
    LocalNodeStateSummary s;
    s.m_nodeId = nodeId;
    s.m_endpoint = endpoint;
    s.m_dataDirectory = config.rootPath();

    if (!config.isValid()) {
        s.m_readable = false;
        s.m_readError = "data directory config is invalid";
        return s;
    }

    const NodeDataDirectoryReadResult result = NodeDataDirectory::loadManifest(config);

    if (!result.loaded()) {
        s.m_readable = false;
        s.m_readError = result.reason();
        return s;
    }

    const NodeRuntimeManifest& manifest = result.manifest();
    s.m_networkName = manifest.networkName();
    s.m_genesisId = manifest.genesisConfigId();
    s.m_latestHeight = manifest.latestBlockHeight();
    s.m_latestBlockHash = manifest.latestBlockHash();
    s.m_latestStateRoot = manifest.latestStateRoot();
    s.m_readable = true;
    s.m_readError.clear();

    return s;
}

LocalNodeStateSummary LocalNodeStateSummary::fromIdentity(const LocalNodeIdentity& identity) {
    return fromDataDirectory(
        NodeDataDirectoryConfig(identity.dataDirectory()),
        identity.nodeId(),
        identity.endpoint()
    );
}

const std::string& LocalNodeStateSummary::nodeId() const { return m_nodeId; }
const std::string& LocalNodeStateSummary::endpoint() const { return m_endpoint; }
const std::filesystem::path& LocalNodeStateSummary::dataDirectory() const { return m_dataDirectory; }
const std::string& LocalNodeStateSummary::networkName() const { return m_networkName; }
const std::string& LocalNodeStateSummary::genesisId() const { return m_genesisId; }
std::uint64_t LocalNodeStateSummary::latestHeight() const { return m_latestHeight; }
const std::string& LocalNodeStateSummary::latestBlockHash() const { return m_latestBlockHash; }
const std::string& LocalNodeStateSummary::latestStateRoot() const { return m_latestStateRoot; }
bool LocalNodeStateSummary::isReadable() const { return m_readable; }
const std::string& LocalNodeStateSummary::readError() const { return m_readError; }

std::string LocalNodeStateSummary::serialize() const {
    std::ostringstream oss;
    oss << "LocalNodeStateSummary{"
        << "nodeId=" << m_nodeId
        << ";endpoint=" << m_endpoint
        << ";networkName=" << m_networkName
        << ";genesisId=" << m_genesisId
        << ";latestHeight=" << m_latestHeight
        << ";latestBlockHash=" << m_latestBlockHash
        << ";readable=" << (m_readable ? "true" : "false");
    if (!m_readable) oss << ";readError=" << m_readError;
    oss << "}";
    return oss.str();
}

} // namespace nodo::node
