#ifndef NODO_NODE_LOCAL_NODE_STATE_SUMMARY_HPP
#define NODO_NODE_LOCAL_NODE_STATE_SUMMARY_HPP

#include "node/LocalNodeIdentity.hpp"
#include "node/NodeDataDirectory.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace nodo::node {

/*
 * LocalNodeStateSummary captures the observable state of one local node
 * derived entirely from its persisted data directory. It does not require
 * a live runtime.
 *
 * Security principle:
 * A summary is a point-in-time snapshot. It does not modify any state.
 * An unreadable or corrupt data directory produces a summary with
 * isReadable() == false and a non-empty readError(). Callers must treat
 * an unreadable summary as divergent, not as absent.
 */
class LocalNodeStateSummary {
public:
    LocalNodeStateSummary();

    // Build a summary from a persisted data directory.
    // nodeId and endpoint are supplied by the caller (from LocalNodeIdentity).
    static LocalNodeStateSummary fromDataDirectory(
        const NodeDataDirectoryConfig& config,
        const std::string& nodeId,
        const std::string& endpoint
    );

    // Build directly from a LocalNodeIdentity.
    static LocalNodeStateSummary fromIdentity(const LocalNodeIdentity& identity);

    const std::string& nodeId() const;
    const std::string& endpoint() const;
    const std::filesystem::path& dataDirectory() const;
    const std::string& networkName() const;
    const std::string& genesisId() const;
    std::uint64_t latestHeight() const;
    const std::string& latestBlockHash() const;
    const std::string& latestStateRoot() const;

    bool isReadable() const;
    const std::string& readError() const;

    std::string serialize() const;

private:
    std::string m_nodeId;
    std::string m_endpoint;
    std::filesystem::path m_dataDirectory;
    std::string m_networkName;
    std::string m_genesisId;
    std::uint64_t m_latestHeight;
    std::string m_latestBlockHash;
    std::string m_latestStateRoot;
    bool m_readable;
    std::string m_readError;
};

} // namespace nodo::node

#endif
