#include "economics/GovernancePolicy.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernancePolicy;

void testValidPolicyAccepted() {
    const GovernancePolicy policy(
        "governance-v1", 10, 5, true, false
    );
    assert(policy.isValid());
    assert(policy.rejectionReason().empty());
    assert(policy.policyVersion() == "governance-v1");
    assert(policy.reviewPeriodBlocks() == 10);
    assert(policy.decisionTimelockBlocks() == 5);
    assert(policy.requireDecisionProof() == true);
    assert(policy.allowEmergencyApproval() == false);
}

void testEmptyPolicyVersionRejected() {
    const GovernancePolicy policy("", 10, 5, true, false);
    assert(!policy.isValid());
    assert(policy.rejectionReason().find("policyVersion") != std::string::npos);
}

void testDefaultConstructedRejected() {
    const GovernancePolicy policy;
    assert(!policy.isValid());
}

void testReviewPeriodZeroAllowed() {
    const GovernancePolicy policy("gov-v1", 0, 0, false, false);
    assert(policy.isValid());
    assert(policy.reviewPeriodBlocks() == 0);
}

void testRequireDecisionProofExposed() {
    const GovernancePolicy policy("gov-v1", 0, 0, true, false);
    assert(policy.requireDecisionProof() == true);
}

void testAllowEmergencyApprovalExposed() {
    const GovernancePolicy policy("gov-v1", 0, 0, false, true);
    assert(policy.allowEmergencyApproval() == true);
}

void testSerializationNotEmpty() {
    const GovernancePolicy policy("gov-v1", 10, 5, true, false);
    const std::string s = policy.serialize();
    assert(!s.empty());
    assert(s.find("gov-v1") != std::string::npos);
}

} // namespace

int main() {
    testValidPolicyAccepted();
    testEmptyPolicyVersionRejected();
    testDefaultConstructedRejected();
    testReviewPeriodZeroAllowed();
    testRequireDecisionProofExposed();
    testAllowEmergencyApprovalExposed();
    testSerializationNotEmpty();
    return 0;
}
