#ifndef NODO_NODE_PROTOCOL_INVARIANT_CHECKER_HPP
#define NODO_NODE_PROTOCOL_INVARIANT_CHECKER_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"

#include <cstddef>
#include <string>

namespace nodo::node {

class ProtocolInvariantCheckResult {
public:
    ProtocolInvariantCheckResult();

    static ProtocolInvariantCheckResult passed(
        std::size_t checkedInvariantCount
    );

    static ProtocolInvariantCheckResult failed(
        std::string reason,
        std::size_t checkedInvariantCount
    );

    bool passed() const;
    const std::string& reason() const;
    std::size_t checkedInvariantCount() const;

    std::string serialize() const;

private:
    bool m_passed;
    std::string m_reason;
    std::size_t m_checkedInvariantCount;
};

class ProtocolInvariantChecker {
public:
    static ProtocolInvariantCheckResult checkRuntime(
        const NodeRuntime& runtime
    );

    static ProtocolInvariantCheckResult checkRuntimeAgainstManifest(
        const NodeRuntime& runtime,
        const NodeRuntimeManifest& manifest
    );

    static ProtocolInvariantCheckResult checkPenaltyLedger(
        const consensus::ValidatorPenaltyLedger& ledger
    );
};

} // namespace nodo::node

#endif
