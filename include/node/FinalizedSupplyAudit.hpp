#ifndef NODO_NODE_FINALIZED_SUPPLY_AUDIT_HPP
#define NODO_NODE_FINALIZED_SUPPLY_AUDIT_HPP

#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class FinalizedSupplyAuditResult {
public:
    FinalizedSupplyAuditResult();

    static FinalizedSupplyAuditResult passed(
        utils::Amount finalSupply,
        std::size_t deltaCount
    );

    static FinalizedSupplyAuditResult failed(
        std::string reason,
        std::uint64_t failedBlockHeight
    );

    bool passed() const;
    const std::string& reason() const;
    utils::Amount finalSupply() const;
    std::size_t deltaCount() const;
    std::uint64_t failedBlockHeight() const;

    std::string serialize() const;

private:
    bool m_passed;
    std::string m_reason;
    utils::Amount m_finalSupply;
    std::size_t m_deltaCount;
    std::uint64_t m_failedBlockHeight;
};

class FinalizedSupplyAudit {
public:
    static FinalizedSupplyAuditResult auditArtifacts(
        const economics::MonetaryPolicy& policy,
        const std::vector<FinalizedBlockArtifact>& artifacts
    );

    static FinalizedSupplyAuditResult auditDeltas(
        const economics::MonetaryPolicy& policy,
        const std::vector<economics::SupplyDelta>& deltas
    );
};

} // namespace nodo::node

#endif
