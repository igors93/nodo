#include "economics/TreasuryApprovalProof.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryApprovalProof;

void testProofIsNonEmpty() {
    const std::string proof = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    assert(!proof.empty());
}

void testSameInputsProduceSameProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    assert(a == b);
}

void testDifferentDecisionIdChangesProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-002", "governance-v1", 20
    );
    assert(a != b);
}

void testDifferentProposalIdChangesProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-002", "decision-001", "governance-v1", 20
    );
    assert(a != b);
}

void testDifferentPolicyVersionChangesProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v2", 20
    );
    assert(a != b);
}

void testDifferentGovernanceProposalIdChangesProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-002", "prop-001", "decision-001", "governance-v1", 20
    );
    assert(a != b);
}

void testDifferentBlockChangesProof() {
    const std::string a = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const std::string b = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 21
    );
    assert(a != b);
}

void testProofContainsExpectedPrefix() {
    const std::string proof = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    assert(proof.find("treasury-approval:") == 0);
}

} // namespace

int main() {
    testProofIsNonEmpty();
    testSameInputsProduceSameProof();
    testDifferentDecisionIdChangesProof();
    testDifferentProposalIdChangesProof();
    testDifferentPolicyVersionChangesProof();
    testDifferentGovernanceProposalIdChangesProof();
    testDifferentBlockChangesProof();
    testProofContainsExpectedPrefix();
    return 0;
}
