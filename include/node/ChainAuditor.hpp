#ifndef NODO_NODE_CHAIN_AUDITOR_HPP
#define NODO_NODE_CHAIN_AUDITOR_HPP

#include "node/ChainAuditResult.hpp"

namespace nodo::node {

class RuntimeStateLoadResult;

class ChainAuditor {
public:
    static ChainAuditResult auditLoadedRuntime(
        const RuntimeStateLoadResult& load
    );
};

} // namespace nodo::node

#endif
