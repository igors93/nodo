#ifndef NODO_NODE_FINALIZED_SLASHING_EVIDENCE_AUDIT_HPP
#define NODO_NODE_FINALIZED_SLASHING_EVIDENCE_AUDIT_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/Block.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/StakingRegistry.hpp"

#include <cstddef>
#include <string>

namespace nodo::node {

class FinalizedSlashingEvidenceAuditResult {
public:
    static FinalizedSlashingEvidenceAuditResult passed(std::size_t evidenceCount);
    static FinalizedSlashingEvidenceAuditResult failed(std::string reason);

    bool passed() const;
    const std::string& reason() const;
    std::size_t evidenceCount() const;

private:
    bool m_passed = false;
    std::string m_reason;
    std::size_t m_evidenceCount = 0;
};

/*
 * Verifies that every finalized SLASHING_EVIDENCE ledger record in a block has
 * already produced the deterministic protocol consequence in all slashing
 * domains.  This is the audit boundary used by reload, finalized-block sync and
 * artifact import so a peer that missed the original gossip still proves the
 * finalized evidence through the block payload and replayed state.
 */
class FinalizedSlashingEvidenceAudit {
public:
    static FinalizedSlashingEvidenceAuditResult auditBlockEffects(
        const core::Block& block,
        const consensus::ValidatorPenaltyLedger& penaltyLedger,
        const core::ValidatorRegistry& validators,
        const StakingRegistry& staking
    );
};

} // namespace nodo::node

#endif
