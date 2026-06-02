#include "economics/GovernanceDecisionRecord.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;

GovernanceDecisionRecord validApprovedDecision(
    const std::string& policyVersion = "governance-v1"
) {
    return GovernanceDecisionRecord(
        "decision-001",
        "gov-prop-001",
        "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20,
        "governance-node",
        "decision-proof-xyz",
        policyVersion
    );
}

void testValidApprovedDecisionAccepted() {
    const auto dec = validApprovedDecision();
    assert(dec.isValid());
    assert(dec.rejectionReason().empty());
    assert(dec.decisionId() == "decision-001");
    assert(dec.governanceProposalId() == "gov-prop-001");
    assert(dec.proposalType() == "TREASURY_SPEND");
    assert(dec.decisionStatus() == GovernanceDecisionStatus::APPROVED);
    assert(dec.decidedAtBlock() == 20);
    assert(dec.decisionMaker() == "governance-node");
    assert(dec.decisionProof() == "decision-proof-xyz");
    assert(dec.policyVersion() == "governance-v1");
    assert(dec.approved() == true);
}

void testRejectedDecisionIsValidButNotApproved() {
    const GovernanceDecisionRecord dec(
        "decision-002", "gov-prop-002", "TREASURY_SPEND",
        GovernanceDecisionStatus::REJECTED,
        20, "governance-node", "", "governance-v1"
    );
    assert(dec.isValid());
    assert(dec.approved() == false);
}

void testExpiredDecisionIsValidButNotApproved() {
    const GovernanceDecisionRecord dec(
        "decision-003", "gov-prop-003", "TREASURY_SPEND",
        GovernanceDecisionStatus::EXPIRED,
        20, "governance-node", "", "governance-v1"
    );
    assert(dec.isValid());
    assert(dec.approved() == false);
}

void testCancelledDecisionIsValidButNotApproved() {
    const GovernanceDecisionRecord dec(
        "decision-004", "gov-prop-004", "TREASURY_SPEND",
        GovernanceDecisionStatus::CANCELLED,
        20, "governance-node", "", "governance-v1"
    );
    assert(dec.isValid());
    assert(dec.approved() == false);
}

void testEmptyDecisionIdRejected() {
    const GovernanceDecisionRecord dec(
        "", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "proof", "governance-v1"
    );
    assert(!dec.isValid());
    assert(dec.rejectionReason().find("decisionId") != std::string::npos);
}

void testEmptyGovernanceProposalIdRejected() {
    const GovernanceDecisionRecord dec(
        "decision-001", "", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "proof", "governance-v1"
    );
    assert(!dec.isValid());
    assert(dec.rejectionReason().find("governanceProposalId") != std::string::npos);
}

void testEmptyProposalTypeRejected() {
    const GovernanceDecisionRecord dec(
        "decision-001", "gov-prop-001", "",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "proof", "governance-v1"
    );
    assert(!dec.isValid());
    assert(dec.rejectionReason().find("proposalType") != std::string::npos);
}

void testEmptyDecisionMakerRejected() {
    const GovernanceDecisionRecord dec(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "", "proof", "governance-v1"
    );
    assert(!dec.isValid());
    assert(dec.rejectionReason().find("decisionMaker") != std::string::npos);
}

void testEmptyPolicyVersionRejected() {
    const GovernanceDecisionRecord dec(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "proof", ""
    );
    assert(!dec.isValid());
    assert(dec.rejectionReason().find("policyVersion") != std::string::npos);
}

void testEmptyDecisionProofAllowedAtRecordLevel() {
    // The decision record itself does not enforce requireDecisionProof.
    // That check belongs to GovernanceApprovalBridge.
    const GovernanceDecisionRecord dec(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "", "governance-v1"
    );
    assert(dec.isValid());
}

void testDefaultConstructedRejected() {
    const GovernanceDecisionRecord dec;
    assert(!dec.isValid());
}

void testStatusToString() {
    assert(nodo::economics::governanceDecisionStatusToString(
               GovernanceDecisionStatus::APPROVED) == "APPROVED");
    assert(nodo::economics::governanceDecisionStatusToString(
               GovernanceDecisionStatus::REJECTED) == "REJECTED");
    assert(nodo::economics::governanceDecisionStatusToString(
               GovernanceDecisionStatus::EXPIRED) == "EXPIRED");
    assert(nodo::economics::governanceDecisionStatusToString(
               GovernanceDecisionStatus::CANCELLED) == "CANCELLED");
}

void testStatusFromString() {
    GovernanceDecisionStatus s;
    assert(nodo::economics::governanceDecisionStatusFromString("APPROVED", s));
    assert(s == GovernanceDecisionStatus::APPROVED);
    assert(!nodo::economics::governanceDecisionStatusFromString("UNKNOWN_STATUS", s));
}

void testSerializationNotEmpty() {
    const auto dec = validApprovedDecision();
    const std::string s = dec.serialize();
    assert(!s.empty());
    assert(s.find("decision-001") != std::string::npos);
    assert(s.find("APPROVED") != std::string::npos);
}

} // namespace

int main() {
    testValidApprovedDecisionAccepted();
    testRejectedDecisionIsValidButNotApproved();
    testExpiredDecisionIsValidButNotApproved();
    testCancelledDecisionIsValidButNotApproved();
    testEmptyDecisionIdRejected();
    testEmptyGovernanceProposalIdRejected();
    testEmptyProposalTypeRejected();
    testEmptyDecisionMakerRejected();
    testEmptyPolicyVersionRejected();
    testEmptyDecisionProofAllowedAtRecordLevel();
    testDefaultConstructedRejected();
    testStatusToString();
    testStatusFromString();
    testSerializationNotEmpty();
    return 0;
}
