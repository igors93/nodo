#ifndef NODO_NODE_CHAIN_AUDITOR_HPP
#define NODO_NODE_CHAIN_AUDITOR_HPP

#include "node/ChainAuditResult.hpp"

#include <filesystem>

namespace nodo::node {

class RuntimeStateLoadResult;

class ChainAuditor {
public:
    /*
     * Audit a loaded runtime.
     *
     * When finalized blocks exist, monetaryReportPath must be non-empty and
     * the persisted epoch monetary report at that path is loaded and compared
     * against the report rebuilt from finalized SupplyDeltas. An empty path
     * when finalized blocks exist causes the audit to fail.
     *
     * Security principle:
     * No state that cannot be verified from finalized data is accepted.
     * Report verification is mandatory, not optional, in the production path.
     */
    static ChainAuditResult auditLoadedRuntime(
        const RuntimeStateLoadResult& load,
        const std::filesystem::path& monetaryReportPath = {}
    );

    /*
     * Audit a loaded runtime without requiring a persisted monetary report.
     *
     * This helper is intended for development/testing only. In production,
     * always use auditLoadedRuntime with an explicit monetaryReportPath.
     * The name makes the intentional skip visible at every call site.
     */
    static ChainAuditResult auditLoadedRuntimeDevMode(
        const RuntimeStateLoadResult& load
    );
};

} // namespace nodo::node

#endif
