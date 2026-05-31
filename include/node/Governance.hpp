#ifndef NODO_NODE_GOVERNANCE_HPP
#define NODO_NODE_GOVERNANCE_HPP

#include "node/ControlledIssuance.hpp"
#include "node/ProtectionTreasury.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class GovernancePolicySnapshot {
public:
    GovernancePolicySnapshot();

    GovernancePolicySnapshot(
        std::string status,
        std::uint64_t blockHeight,
        std::uint32_t requiredApprovalBasisPoints,
        std::uint64_t timelockBlocks,
        std::uint64_t activationDelayBlocks,
        std::string policyId,
        std::string reason
    );

    static GovernancePolicySnapshot notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint32_t requiredApprovalBasisPoints() const;
    std::uint64_t timelockBlocks() const;
    std::uint64_t activationDelayBlocks() const;
    const std::string& policyId() const;
    const std::string& reason() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint32_t m_requiredApprovalBasisPoints;
    std::uint64_t m_timelockBlocks;
    std::uint64_t m_activationDelayBlocks;
    std::string m_policyId;
    std::string m_reason;
};

class GovernanceActionGuard {
public:
    GovernanceActionGuard();

    GovernanceActionGuard(
        std::string actionType,
        std::string status,
        std::uint64_t blockHeight,
        std::string protectedResource,
        std::uint32_t requiredApprovalBasisPoints,
        std::uint64_t timelockBlocks,
        std::string reason,
        std::string sourcePolicyDigest
    );

    const std::string& actionType() const;
    const std::string& status() const;
    std::uint64_t blockHeight() const;
    const std::string& protectedResource() const;
    std::uint32_t requiredApprovalBasisPoints() const;
    std::uint64_t timelockBlocks() const;
    const std::string& reason() const;
    const std::string& sourcePolicyDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_actionType;
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::string m_protectedResource;
    std::uint32_t m_requiredApprovalBasisPoints;
    std::uint64_t m_timelockBlocks;
    std::string m_reason;
    std::string m_sourcePolicyDigest;
};

class GovernanceSummary {
public:
    GovernanceSummary();

    GovernanceSummary(
        std::string status,
        std::uint64_t blockHeight,
        std::uint64_t guardCount,
        std::uint64_t activeProposalCount,
        std::uint64_t approvedProposalCount,
        std::uint64_t executableProposalCount,
        std::uint64_t executedProposalCount,
        std::string reason,
        std::string sourceGuardDigest
    );

    static GovernanceSummary notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint64_t guardCount() const;
    std::uint64_t activeProposalCount() const;
    std::uint64_t approvedProposalCount() const;
    std::uint64_t executableProposalCount() const;
    std::uint64_t executedProposalCount() const;
    const std::string& reason() const;
    const std::string& sourceGuardDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint64_t m_guardCount;
    std::uint64_t m_activeProposalCount;
    std::uint64_t m_approvedProposalCount;
    std::uint64_t m_executableProposalCount;
    std::uint64_t m_executedProposalCount;
    std::string m_reason;
    std::string m_sourceGuardDigest;
};

class Governance {
public:
    static constexpr const char* POLICY_REASON =
        "GOVERNANCE_POLICY_V1";

    static constexpr const char* ACTION_GUARD_REASON =
        "GOVERNANCE_ACTION_GUARD";

    static constexpr const char* SUMMARY_REASON =
        "GOVERNANCE_NO_ACTIVE_PROPOSALS";

    static constexpr const char* NOT_EVALUATED_REASON =
        "GOVERNANCE_NOT_EVALUATED";

    static constexpr const char* TREASURY_SPEND_ACTION =
        "TREASURY_SPEND";

    static constexpr const char* MINT_AUTHORIZATION_ACTION =
        "MINT_AUTHORIZATION";

    static constexpr const char* TREASURY_RESOURCE =
        "PROTOCOL_TREASURY";

    static constexpr const char* MINT_RESOURCE =
        "CONTROLLED_ISSUANCE";

    static GovernancePolicySnapshot buildPolicySnapshot(
        std::uint64_t blockHeight
    );

    static std::vector<GovernanceActionGuard> buildActionGuards(
        const GovernancePolicySnapshot& policy
    );

    static GovernanceSummary buildSummary(
        std::uint64_t blockHeight,
        const std::vector<GovernanceActionGuard>& guards
    );

    static bool samePolicy(
        const GovernancePolicySnapshot& left,
        const GovernancePolicySnapshot& right
    );

    static bool sameActionGuards(
        const std::vector<GovernanceActionGuard>& left,
        const std::vector<GovernanceActionGuard>& right
    );

    static bool sameSummary(
        const GovernanceSummary& left,
        const GovernanceSummary& right
    );
};

} // namespace nodo::node

#endif
