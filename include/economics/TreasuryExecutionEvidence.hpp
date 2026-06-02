#ifndef NODO_ECONOMICS_TREASURY_EXECUTION_EVIDENCE_HPP
#define NODO_ECONOMICS_TREASURY_EXECUTION_EVIDENCE_HPP

#include "economics/GovernanceLifecycleRecord.hpp"
#include "economics/TreasuryAccount.hpp"
#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryPolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GovernanceApprovalContext carries the verified governance lifecycle used to
 * produce the TreasuryApproval embedded in evidence. Any node can rebuild the
 * tally and decision from votes, re-run GovernanceApprovalBridge, and compare
 * the reproduced approval against the stored approval.
 */
struct GovernanceApprovalContext {
    GovernanceLifecycleRecord governanceLifecycle;
};

/*
 * TreasuryExecutionEvidence binds a TreasurySpendRecord to every piece of
 * context that authorized it: proposal, approval, policy, treasury state
 * before the spend, and the execution context (block height, epoch spend so far).
 *
 * Security principle:
 * A spend record alone does not prove authorization. Evidence carries the full
 * chain so that any node can independently reproduce the spend and verify:
 *   - the proposal was valid and matches the spend;
 *   - the approval was valid and matches the proposal;
 *   - the policy was satisfied;
 *   - the treasury had enough balance;
 *   - the timelock was respected;
 *   - the execution block matches;
 *   - the epoch is correct;
 *   - the approval was produced from a verified governance lifecycle.
 *
 * Any field mismatch between proposal, approval, policy, and spendRecord makes
 * the evidence structurally invalid and must cause artifact rejection.
 *
 * Production path:
 * Evidence must carry a GovernanceApprovalContext so that
 * TreasuryGovernanceEvidenceValidator can prove the TreasuryApproval was
 * produced by verified votes, tally, decision, and bridge output.
 */
class TreasuryExecutionEvidence {
public:
    TreasuryExecutionEvidence();

    // Legacy constructor: no governance context. Structurally valid if all
    // fields are consistent, but rejected by TreasuryExecutionValidator and
    // FinalizedTreasurySectionValidator for missing governance context.
    TreasuryExecutionEvidence(
        std::string evidenceId,
        TreasuryProposal proposal,
        TreasuryApproval approval,
        TreasuryPolicy policy,
        TreasuryAccount treasuryAccountBefore,
        std::uint64_t currentBlockHeight,
        utils::Amount epochSpentSoFar,
        TreasurySpendRecord spendRecord,
        std::int64_t createdAt
    );

    // Canonical constructor: includes governance context proving the approval
    // was produced by GovernanceApprovalBridge.
    TreasuryExecutionEvidence(
        std::string evidenceId,
        TreasuryProposal proposal,
        TreasuryApproval approval,
        TreasuryPolicy policy,
        TreasuryAccount treasuryAccountBefore,
        std::uint64_t currentBlockHeight,
        utils::Amount epochSpentSoFar,
        TreasurySpendRecord spendRecord,
        std::int64_t createdAt,
        GovernanceApprovalContext governanceContext
    );

    const std::string& evidenceId() const;
    const TreasuryProposal& proposal() const;
    const TreasuryApproval& approval() const;
    const TreasuryPolicy& policy() const;
    const TreasuryAccount& treasuryAccountBefore() const;
    std::uint64_t currentBlockHeight() const;
    utils::Amount epochSpentSoFar() const;
    const TreasurySpendRecord& spendRecord() const;
    std::int64_t createdAt() const;

    bool hasGovernanceContext() const;
    const GovernanceApprovalContext& governanceContext() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_evidenceId;
    TreasuryProposal m_proposal;
    TreasuryApproval m_approval;
    TreasuryPolicy m_policy;
    TreasuryAccount m_treasuryAccountBefore;
    std::uint64_t m_currentBlockHeight;
    utils::Amount m_epochSpentSoFar;
    TreasurySpendRecord m_spendRecord;
    std::int64_t m_createdAt;
    bool m_hasGovernanceContext;
    GovernanceApprovalContext m_governanceContext;
    mutable bool m_validated;
    mutable bool m_valid;
    mutable std::string m_rejectionReason;

    void validate() const;
};

} // namespace nodo::economics

#endif
