#include "node/Governance.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::Governance;
using nodo::node::GovernanceActionGuard;
using nodo::node::GovernancePolicySnapshot;
using nodo::node::GovernanceSummary;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testBuildsGovernancePolicy() {
    const GovernancePolicySnapshot policy =
        Governance::buildPolicySnapshot(1);

    requireCondition(
        policy.active() &&
        policy.requiredApprovalBasisPoints() == 8000 &&
        policy.timelockBlocks() >= 10080 &&
        policy.reason() == Governance::POLICY_REASON,
        "Governance policy should require strong approval and timelock."
    );
}

void testBuildsTreasuryAndMintGuards() {
    const GovernancePolicySnapshot policy =
        Governance::buildPolicySnapshot(1);

    const std::vector<GovernanceActionGuard> guards =
        Governance::buildActionGuards(policy);

    requireCondition(
        guards.size() == 2U &&
        guards[0].active() &&
        guards[0].actionType() == Governance::TREASURY_SPEND_ACTION &&
        guards[0].protectedResource() == Governance::TREASURY_RESOURCE &&
        guards[1].active() &&
        guards[1].actionType() == Governance::MINT_AUTHORIZATION_ACTION &&
        guards[1].protectedResource() == Governance::MINT_RESOURCE,
        "Governance should guard treasury spend and mint authorization."
    );
}

void testBuildsGovernanceSummary() {
    const GovernancePolicySnapshot policy =
        Governance::buildPolicySnapshot(1);

    const std::vector<GovernanceActionGuard> guards =
        Governance::buildActionGuards(policy);

    const GovernanceSummary summary =
        Governance::buildSummary(1, guards);

    requireCondition(
        summary.active() &&
        summary.guardCount() == 2U &&
        summary.activeProposalCount() == 0U &&
        summary.approvedProposalCount() == 0U &&
        summary.executedProposalCount() == 0U &&
        summary.reason() == Governance::SUMMARY_REASON,
        "Governance summary should be active with no executable proposals by default."
    );
}

} // namespace

int main() {
    try {
        testBuildsGovernancePolicy();
        testBuildsTreasuryAndMintGuards();
        testBuildsGovernanceSummary();

        std::cout << "Nodo governance tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo governance tests failed: " << error.what() << "\n";
        return 1;
    }
}
