#ifndef NODO_ECONOMICS_TREASURY_APPROVAL_PROOF_HPP
#define NODO_ECONOMICS_TREASURY_APPROVAL_PROOF_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * TreasuryApprovalProof produces a deterministic proof string from
 * the governance approval context: governance proposal ID, treasury proposal
 * ID, decision ID, policy version, and the block at which the decision
 * was made.
 *
 * Security principle:
 * The proof is computed deterministically from verifiable inputs. Any node
 * holding the same governance context can reproduce the proof and compare
 * it against the stored TreasuryApproval.approvalProof. A mismatch means
 * the approval was not produced by GovernanceApprovalBridge.
 *
 * Format:
 *   treasury-approval:<governanceProposalId>:<treasuryProposalId>
 *                     :<decisionId>:<policyVersion>:<decidedAtBlock>
 */
class TreasuryApprovalProof {
public:
    static std::string build(
        const std::string& governanceProposalId,
        const std::string& treasuryProposalId,
        const std::string& decisionId,
        const std::string& policyVersion,
        std::uint64_t decidedAtBlock
    );
};

} // namespace nodo::economics

#endif
