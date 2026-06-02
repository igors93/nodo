#ifndef NODO_ECONOMICS_GOVERNANCE_DECISION_RECORD_HPP
#define NODO_ECONOMICS_GOVERNANCE_DECISION_RECORD_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

enum class GovernanceDecisionStatus {
    APPROVED,
    REJECTED,
    EXPIRED,
    CANCELLED
};

std::string governanceDecisionStatusToString(GovernanceDecisionStatus status);
bool governanceDecisionStatusFromString(
    const std::string& s,
    GovernanceDecisionStatus& out
);

/*
 * GovernanceDecisionRecord captures the outcome of a governance lifecycle
 * for a specific proposal. It records who decided, when, under which policy,
 * and carries a decisionProof when the policy requires it.
 *
 * Security principle:
 * Only APPROVED decisions may produce a TreasuryApproval through
 * GovernanceApprovalBridge. A decision record with any other status is a
 * valid record but must never result in a TreasuryApproval.
 */
class GovernanceDecisionRecord {
public:
    GovernanceDecisionRecord();

    GovernanceDecisionRecord(
        std::string decisionId,
        std::string governanceProposalId,
        std::string proposalType,
        GovernanceDecisionStatus decisionStatus,
        std::uint64_t decidedAtBlock,
        std::string decisionMaker,
        std::string decisionProof,
        std::string policyVersion
    );

    const std::string& decisionId() const;
    const std::string& governanceProposalId() const;
    const std::string& proposalType() const;
    GovernanceDecisionStatus decisionStatus() const;
    std::uint64_t decidedAtBlock() const;
    const std::string& decisionMaker() const;
    const std::string& decisionProof() const;
    const std::string& policyVersion() const;

    bool approved() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_decisionId;
    std::string m_governanceProposalId;
    std::string m_proposalType;
    GovernanceDecisionStatus m_decisionStatus;
    std::uint64_t m_decidedAtBlock;
    std::string m_decisionMaker;
    std::string m_decisionProof;
    std::string m_policyVersion;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
