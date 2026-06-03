#include "node/LocalNetworkStateInspector.hpp"

#include <sstream>

namespace nodo::node {

std::string localNodeDivergenceKindToString(LocalNodeDivergenceKind kind) {
    switch (kind) {
        case LocalNodeDivergenceKind::NONE:                       return "NONE";
        case LocalNodeDivergenceKind::GENESIS_MISMATCH:           return "GENESIS_MISMATCH";
        case LocalNodeDivergenceKind::SAME_HEIGHT_DIFFERENT_HASH: return "SAME_HEIGHT_DIFFERENT_HASH";
        case LocalNodeDivergenceKind::DIFFERENT_HEIGHT:           return "DIFFERENT_HEIGHT";
        case LocalNodeDivergenceKind::UNREADABLE_STATE:           return "UNREADABLE_STATE";
        default:                                                  return "UNREADABLE_STATE";
    }
}

LocalNodeDivergenceReport::LocalNodeDivergenceReport()
    : m_divergent(false),
      m_kind(LocalNodeDivergenceKind::NONE),
      m_nodeIdA(),
      m_nodeIdB(),
      m_reason() {}

LocalNodeDivergenceReport LocalNodeDivergenceReport::aligned(
    const std::string& nodeIdA, const std::string& nodeIdB
) {
    LocalNodeDivergenceReport r;
    r.m_divergent = false;
    r.m_kind = LocalNodeDivergenceKind::NONE;
    r.m_nodeIdA = nodeIdA;
    r.m_nodeIdB = nodeIdB;
    return r;
}

LocalNodeDivergenceReport LocalNodeDivergenceReport::divergent(
    LocalNodeDivergenceKind kind,
    const std::string& nodeIdA,
    const std::string& nodeIdB,
    std::string reason
) {
    LocalNodeDivergenceReport r;
    r.m_divergent = true;
    r.m_kind = kind;
    r.m_nodeIdA = nodeIdA;
    r.m_nodeIdB = nodeIdB;
    r.m_reason = std::move(reason);
    return r;
}

bool LocalNodeDivergenceReport::isDivergent() const { return m_divergent; }
LocalNodeDivergenceKind LocalNodeDivergenceReport::kind() const { return m_kind; }
const std::string& LocalNodeDivergenceReport::nodeIdA() const { return m_nodeIdA; }
const std::string& LocalNodeDivergenceReport::nodeIdB() const { return m_nodeIdB; }
const std::string& LocalNodeDivergenceReport::reason() const { return m_reason; }

std::string LocalNodeDivergenceReport::serialize() const {
    std::ostringstream oss;
    oss << "LocalNodeDivergenceReport{"
        << "divergent=" << (m_divergent ? "true" : "false")
        << ";kind=" << localNodeDivergenceKindToString(m_kind)
        << ";nodeIdA=" << m_nodeIdA
        << ";nodeIdB=" << m_nodeIdB;
    if (m_divergent) oss << ";reason=" << m_reason;
    oss << "}";
    return oss.str();
}

// ---- LocalNetworkStateInspector ----

std::vector<LocalNodeStateSummary> LocalNetworkStateInspector::summarize(
    const LocalPeerTopology& topology
) {
    std::vector<LocalNodeStateSummary> summaries;
    summaries.reserve(topology.size());

    for (const auto& identity : topology.nodes()) {
        summaries.push_back(LocalNodeStateSummary::fromIdentity(identity));
    }

    return summaries;
}

LocalNodeDivergenceReport LocalNetworkStateInspector::compareNodes(
    const LocalNodeStateSummary& a,
    const LocalNodeStateSummary& b
) {
    // Unreadable state is always divergent.
    if (!a.isReadable()) {
        return LocalNodeDivergenceReport::divergent(
            LocalNodeDivergenceKind::UNREADABLE_STATE,
            a.nodeId(), b.nodeId(),
            "node '" + a.nodeId() + "' state is unreadable: " + a.readError()
        );
    }

    if (!b.isReadable()) {
        return LocalNodeDivergenceReport::divergent(
            LocalNodeDivergenceKind::UNREADABLE_STATE,
            a.nodeId(), b.nodeId(),
            "node '" + b.nodeId() + "' state is unreadable: " + b.readError()
        );
    }

    // Genesis mismatch.
    if (!a.genesisId().empty() && !b.genesisId().empty() &&
        a.genesisId() != b.genesisId()) {
        return LocalNodeDivergenceReport::divergent(
            LocalNodeDivergenceKind::GENESIS_MISMATCH,
            a.nodeId(), b.nodeId(),
            "genesis mismatch: '" + a.genesisId() + "' vs '" + b.genesisId() + "'"
        );
    }

    // Same height but different block hash — a fork.
    if (a.latestHeight() == b.latestHeight() &&
        !a.latestBlockHash().empty() && !b.latestBlockHash().empty() &&
        a.latestBlockHash() != b.latestBlockHash()) {
        return LocalNodeDivergenceReport::divergent(
            LocalNodeDivergenceKind::SAME_HEIGHT_DIFFERENT_HASH,
            a.nodeId(), b.nodeId(),
            "nodes are at the same height " + std::to_string(a.latestHeight()) +
            " with different block hashes"
        );
    }

    // Different heights.
    if (a.latestHeight() != b.latestHeight()) {
        return LocalNodeDivergenceReport::divergent(
            LocalNodeDivergenceKind::DIFFERENT_HEIGHT,
            a.nodeId(), b.nodeId(),
            "height divergence: node '" + a.nodeId() + "' at " +
            std::to_string(a.latestHeight()) + ", node '" + b.nodeId() +
            "' at " + std::to_string(b.latestHeight())
        );
    }

    return LocalNodeDivergenceReport::aligned(a.nodeId(), b.nodeId());
}

std::vector<LocalNodeDivergenceReport> LocalNetworkStateInspector::findDivergence(
    const std::vector<LocalNodeStateSummary>& summaries
) {
    std::vector<LocalNodeDivergenceReport> reports;

    for (std::size_t i = 0; i < summaries.size(); ++i) {
        for (std::size_t j = i + 1; j < summaries.size(); ++j) {
            const LocalNodeDivergenceReport report =
                compareNodes(summaries[i], summaries[j]);

            if (report.isDivergent()) {
                reports.push_back(report);
            }
        }
    }

    return reports;
}

} // namespace nodo::node
