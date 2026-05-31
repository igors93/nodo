#include "node/Governance.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

GovernancePolicySnapshot::GovernancePolicySnapshot()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_requiredApprovalBasisPoints(0),
      m_timelockBlocks(0),
      m_activationDelayBlocks(0),
      m_policyId(""),
      m_reason(Governance::NOT_EVALUATED_REASON) {}

GovernancePolicySnapshot::GovernancePolicySnapshot(
    std::string status,
    std::uint64_t blockHeight,
    std::uint32_t requiredApprovalBasisPoints,
    std::uint64_t timelockBlocks,
    std::uint64_t activationDelayBlocks,
    std::string policyId,
    std::string reason
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_requiredApprovalBasisPoints(requiredApprovalBasisPoints),
      m_timelockBlocks(timelockBlocks),
      m_activationDelayBlocks(activationDelayBlocks),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)) {}

GovernancePolicySnapshot GovernancePolicySnapshot::notEvaluated() {
    return GovernancePolicySnapshot();
}

const std::string& GovernancePolicySnapshot::status() const { return m_status; }
std::uint64_t GovernancePolicySnapshot::blockHeight() const { return m_blockHeight; }
std::uint32_t GovernancePolicySnapshot::requiredApprovalBasisPoints() const { return m_requiredApprovalBasisPoints; }
std::uint64_t GovernancePolicySnapshot::timelockBlocks() const { return m_timelockBlocks; }
std::uint64_t GovernancePolicySnapshot::activationDelayBlocks() const { return m_activationDelayBlocks; }
const std::string& GovernancePolicySnapshot::policyId() const { return m_policyId; }
const std::string& GovernancePolicySnapshot::reason() const { return m_reason; }

bool GovernancePolicySnapshot::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool GovernancePolicySnapshot::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_requiredApprovalBasisPoints == 0 &&
               m_timelockBlocks == 0 &&
               m_activationDelayBlocks == 0 &&
               m_policyId.empty() &&
               m_reason == Governance::NOT_EVALUATED_REASON;
    }

    return m_status == "ACTIVE" &&
           m_blockHeight > 0 &&
           m_requiredApprovalBasisPoints >= ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS &&
           m_requiredApprovalBasisPoints <= 10000 &&
           m_timelockBlocks >= NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS &&
           m_activationDelayBlocks >= m_timelockBlocks &&
           !m_policyId.empty() &&
           m_reason == Governance::POLICY_REASON;
}

std::string GovernancePolicySnapshot::serialize() const {
    std::ostringstream oss;
    oss << "GovernancePolicySnapshot{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";requiredApprovalBasisPoints=" << m_requiredApprovalBasisPoints
        << ";timelockBlocks=" << m_timelockBlocks
        << ";activationDelayBlocks=" << m_activationDelayBlocks
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

GovernanceActionGuard::GovernanceActionGuard()
    : m_actionType(""),
      m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_protectedResource(""),
      m_requiredApprovalBasisPoints(0),
      m_timelockBlocks(0),
      m_reason(Governance::NOT_EVALUATED_REASON),
      m_sourcePolicyDigest("") {}

GovernanceActionGuard::GovernanceActionGuard(
    std::string actionType,
    std::string status,
    std::uint64_t blockHeight,
    std::string protectedResource,
    std::uint32_t requiredApprovalBasisPoints,
    std::uint64_t timelockBlocks,
    std::string reason,
    std::string sourcePolicyDigest
)
    : m_actionType(std::move(actionType)),
      m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_protectedResource(std::move(protectedResource)),
      m_requiredApprovalBasisPoints(requiredApprovalBasisPoints),
      m_timelockBlocks(timelockBlocks),
      m_reason(std::move(reason)),
      m_sourcePolicyDigest(std::move(sourcePolicyDigest)) {}

const std::string& GovernanceActionGuard::actionType() const { return m_actionType; }
const std::string& GovernanceActionGuard::status() const { return m_status; }
std::uint64_t GovernanceActionGuard::blockHeight() const { return m_blockHeight; }
const std::string& GovernanceActionGuard::protectedResource() const { return m_protectedResource; }
std::uint32_t GovernanceActionGuard::requiredApprovalBasisPoints() const { return m_requiredApprovalBasisPoints; }
std::uint64_t GovernanceActionGuard::timelockBlocks() const { return m_timelockBlocks; }
const std::string& GovernanceActionGuard::reason() const { return m_reason; }
const std::string& GovernanceActionGuard::sourcePolicyDigest() const { return m_sourcePolicyDigest; }

bool GovernanceActionGuard::active() const {
    return m_status == "LOCKED" && isValid();
}

bool GovernanceActionGuard::isValid() const {
    const bool knownAction =
        (m_actionType == Governance::TREASURY_SPEND_ACTION && m_protectedResource == Governance::TREASURY_RESOURCE) ||
        (m_actionType == Governance::MINT_AUTHORIZATION_ACTION && m_protectedResource == Governance::MINT_RESOURCE);

    return knownAction &&
           m_status == "LOCKED" &&
           m_blockHeight > 0 &&
           m_requiredApprovalBasisPoints >= ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS &&
           m_requiredApprovalBasisPoints <= 10000 &&
           m_timelockBlocks >= NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS &&
           m_reason == Governance::ACTION_GUARD_REASON &&
           !m_sourcePolicyDigest.empty();
}

std::string GovernanceActionGuard::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceActionGuard{"
        << "actionType=" << m_actionType
        << ";status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";protectedResource=" << m_protectedResource
        << ";requiredApprovalBasisPoints=" << m_requiredApprovalBasisPoints
        << ";timelockBlocks=" << m_timelockBlocks
        << ";reason=" << m_reason
        << ";sourcePolicyDigest=" << m_sourcePolicyDigest
        << "}";
    return oss.str();
}

GovernanceSummary::GovernanceSummary()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_guardCount(0),
      m_activeProposalCount(0),
      m_approvedProposalCount(0),
      m_executableProposalCount(0),
      m_executedProposalCount(0),
      m_reason(Governance::NOT_EVALUATED_REASON),
      m_sourceGuardDigest("") {}

GovernanceSummary::GovernanceSummary(
    std::string status,
    std::uint64_t blockHeight,
    std::uint64_t guardCount,
    std::uint64_t activeProposalCount,
    std::uint64_t approvedProposalCount,
    std::uint64_t executableProposalCount,
    std::uint64_t executedProposalCount,
    std::string reason,
    std::string sourceGuardDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_guardCount(guardCount),
      m_activeProposalCount(activeProposalCount),
      m_approvedProposalCount(approvedProposalCount),
      m_executableProposalCount(executableProposalCount),
      m_executedProposalCount(executedProposalCount),
      m_reason(std::move(reason)),
      m_sourceGuardDigest(std::move(sourceGuardDigest)) {}

GovernanceSummary GovernanceSummary::notEvaluated() { return GovernanceSummary(); }

const std::string& GovernanceSummary::status() const { return m_status; }
std::uint64_t GovernanceSummary::blockHeight() const { return m_blockHeight; }
std::uint64_t GovernanceSummary::guardCount() const { return m_guardCount; }
std::uint64_t GovernanceSummary::activeProposalCount() const { return m_activeProposalCount; }
std::uint64_t GovernanceSummary::approvedProposalCount() const { return m_approvedProposalCount; }
std::uint64_t GovernanceSummary::executableProposalCount() const { return m_executableProposalCount; }
std::uint64_t GovernanceSummary::executedProposalCount() const { return m_executedProposalCount; }
const std::string& GovernanceSummary::reason() const { return m_reason; }
const std::string& GovernanceSummary::sourceGuardDigest() const { return m_sourceGuardDigest; }

bool GovernanceSummary::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool GovernanceSummary::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == Governance::NOT_EVALUATED_REASON;
    }

    return m_status == "ACTIVE" &&
           m_blockHeight > 0 &&
           m_guardCount >= 2 &&
           m_activeProposalCount == 0 &&
           m_approvedProposalCount == 0 &&
           m_executableProposalCount == 0 &&
           m_executedProposalCount == 0 &&
           m_reason == Governance::SUMMARY_REASON &&
           !m_sourceGuardDigest.empty();
}

std::string GovernanceSummary::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceSummary{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";guardCount=" << m_guardCount
        << ";activeProposalCount=" << m_activeProposalCount
        << ";approvedProposalCount=" << m_approvedProposalCount
        << ";executableProposalCount=" << m_executableProposalCount
        << ";executedProposalCount=" << m_executedProposalCount
        << ";reason=" << m_reason
        << ";sourceGuardDigest=" << m_sourceGuardDigest
        << "}";
    return oss.str();
}

GovernancePolicySnapshot Governance::buildPolicySnapshot(
    std::uint64_t blockHeight
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build governance policy at genesis height.");
    }

    return GovernancePolicySnapshot(
        "ACTIVE",
        blockHeight,
        ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS,
        NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS,
        NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS,
        "NODO_GOVERNANCE_POLICY_V1",
        POLICY_REASON
    );
}

std::vector<GovernanceActionGuard> Governance::buildActionGuards(
    const GovernancePolicySnapshot& policy
) {
    if (!policy.active()) {
        throw std::invalid_argument("Cannot build governance guards from inactive policy.");
    }

    return {
        GovernanceActionGuard(
            TREASURY_SPEND_ACTION,
            "LOCKED",
            policy.blockHeight(),
            TREASURY_RESOURCE,
            policy.requiredApprovalBasisPoints(),
            policy.timelockBlocks(),
            ACTION_GUARD_REASON,
            policy.serialize()
        ),
        GovernanceActionGuard(
            MINT_AUTHORIZATION_ACTION,
            "LOCKED",
            policy.blockHeight(),
            MINT_RESOURCE,
            policy.requiredApprovalBasisPoints(),
            policy.timelockBlocks(),
            ACTION_GUARD_REASON,
            policy.serialize()
        )
    };
}

GovernanceSummary Governance::buildSummary(
    std::uint64_t blockHeight,
    const std::vector<GovernanceActionGuard>& guards
) {
    if (blockHeight == 0 || guards.empty()) {
        throw std::invalid_argument("Cannot build governance summary without active guards.");
    }

    std::ostringstream digest;
    for (const GovernanceActionGuard& guard : guards) {
        if (!guard.active()) {
            throw std::invalid_argument("Cannot build governance summary from inactive guard.");
        }
        digest << guard.serialize() << '|';
    }

    return GovernanceSummary(
        "ACTIVE",
        blockHeight,
        static_cast<std::uint64_t>(guards.size()),
        0,
        0,
        0,
        0,
        SUMMARY_REASON,
        digest.str()
    );
}

bool Governance::samePolicy(
    const GovernancePolicySnapshot& left,
    const GovernancePolicySnapshot& right
) {
    return left.serialize() == right.serialize();
}

bool Governance::sameActionGuards(
    const std::vector<GovernanceActionGuard>& left,
    const std::vector<GovernanceActionGuard>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

bool Governance::sameSummary(
    const GovernanceSummary& left,
    const GovernanceSummary& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
