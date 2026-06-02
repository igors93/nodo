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
     * When monetaryReportPath is non-empty and finalized blocks exist, the
     * persisted epoch monetary report at that path is loaded and compared
     * against the report rebuilt from finalized SupplyDeltas. Any mismatch
     * or missing file causes the audit to fail.
     */
    static ChainAuditResult auditLoadedRuntime(
        const RuntimeStateLoadResult& load,
        const std::filesystem::path& monetaryReportPath = {}
    );
};

} // namespace nodo::node

#endif
