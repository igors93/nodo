#ifndef NODO_NODE_LOCAL_NETWORK_STATE_INSPECTOR_HPP
#define NODO_NODE_LOCAL_NETWORK_STATE_INSPECTOR_HPP

#include "node/LocalNodeStateSummary.hpp"
#include "node/LocalPeerTopology.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class LocalNodeDivergenceKind {
    NONE,
    GENESIS_MISMATCH,
    SAME_HEIGHT_DIFFERENT_HASH,
    DIFFERENT_HEIGHT,
    UNREADABLE_STATE
};

std::string localNodeDivergenceKindToString(LocalNodeDivergenceKind kind);

/*
 * LocalNodeDivergenceReport describes a detected state difference between
 * two local nodes.
 */
class LocalNodeDivergenceReport {
public:
    LocalNodeDivergenceReport();

    static LocalNodeDivergenceReport aligned(
        const std::string& nodeIdA,
        const std::string& nodeIdB
    );

    static LocalNodeDivergenceReport divergent(
        LocalNodeDivergenceKind kind,
        const std::string& nodeIdA,
        const std::string& nodeIdB,
        std::string reason
    );

    bool isDivergent() const;
    LocalNodeDivergenceKind kind() const;
    const std::string& nodeIdA() const;
    const std::string& nodeIdB() const;
    const std::string& reason() const;

    std::string serialize() const;

private:
    bool m_divergent;
    LocalNodeDivergenceKind m_kind;
    std::string m_nodeIdA;
    std::string m_nodeIdB;
    std::string m_reason;
};

/*
 * LocalNetworkStateInspector builds state summaries for local nodes and
 * detects divergence between them.
 *
 * Security principle:
 * Divergence detection is deterministic and based only on persisted state.
 * An unreadable node is treated as divergent, not as missing. Summaries are
 * produced without modifying any runtime or directory state.
 */
class LocalNetworkStateInspector {
public:
    // Build state summaries for all nodes in a topology.
    static std::vector<LocalNodeStateSummary> summarize(
        const LocalPeerTopology& topology
    );

    // Compare the state of two nodes.
    static LocalNodeDivergenceReport compareNodes(
        const LocalNodeStateSummary& a,
        const LocalNodeStateSummary& b
    );

    // Find all diverging pairs in a collection of summaries.
    static std::vector<LocalNodeDivergenceReport> findDivergence(
        const std::vector<LocalNodeStateSummary>& summaries
    );
};

} // namespace nodo::node

#endif
