#ifndef NODO_NODE_CHAIN_AUDITOR_HPP
#define NODO_NODE_CHAIN_AUDITOR_HPP

#include "node/ChainAuditResult.hpp"
#include "node/RuntimeStateLoader.hpp"

namespace nodo::node {

class ChainAuditor {
public:
    static ChainAuditResult auditLoadedRuntime(
        const RuntimeStateLoadResult& load
    );
};

} // namespace nodo::node

#endif
