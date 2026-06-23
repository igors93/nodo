#ifndef NODO_NODE_PROTOCOL_COMPLETENESS_GATE_HPP
#define NODO_NODE_PROTOCOL_COMPLETENESS_GATE_HPP

#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::node {

enum class ProtocolCompletenessStatus {
    SATISFIED,
    FAILED
};

std::string protocolCompletenessStatusToString(
    ProtocolCompletenessStatus status
);

class ProtocolCompletenessRequirement {
public:
    ProtocolCompletenessRequirement();

    ProtocolCompletenessRequirement(
        std::string id,
        std::string description,
        ProtocolCompletenessStatus status,
        std::string detail
    );

    const std::string& id() const;
    const std::string& description() const;
    ProtocolCompletenessStatus status() const;
    const std::string& detail() const;

    bool satisfied() const;
    std::string serialize() const;

private:
    std::string m_id;
    std::string m_description;
    ProtocolCompletenessStatus m_status;
    std::string m_detail;
};

class ProtocolCompletenessReport {
public:
    ProtocolCompletenessReport();

    explicit ProtocolCompletenessReport(
        std::vector<ProtocolCompletenessRequirement> requirements
    );

    const std::vector<ProtocolCompletenessRequirement>& requirements() const;
    bool complete() const;
    std::size_t satisfiedCount() const;
    std::size_t failedCount() const;
    std::string firstFailure() const;
    std::string serialize() const;

private:
    std::vector<ProtocolCompletenessRequirement> m_requirements;
};

/*
 * ProtocolCompletenessGate turns the high-level node requirements into
 * executable runtime checks. It intentionally composes the consensus,
 * persistence, state, economics, sync, mempool and P2P validators that already
 * own those domains instead of duplicating their rules here.
 */
class ProtocolCompletenessGate {
public:
    static ProtocolCompletenessReport evaluateRuntime(
        const NodeRuntime& runtime
    );

    static ProtocolCompletenessReport evaluateRuntimeWithStorage(
        const NodeRuntime& runtime,
        const NodeDataDirectoryConfig& directoryConfig
    );

private:
    static ProtocolCompletenessReport evaluate(
        const NodeRuntime& runtime,
        const NodeDataDirectoryConfig* directoryConfig
    );
};

} // namespace nodo::node

#endif
