#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernancePolicy;
using nodo::economics::GovernanceProposalEnvelope;
using nodo::economics::TreasuryProposal;
using nodo::utils::Amount;

TreasuryProposal validProposal() {
    return TreasuryProposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

GovernanceProposalEnvelope validEnvelope(
    const std::string& govPolicyVersion = "governance-v1"
) {
    return GovernanceProposalEnvelope(
        "gov-prop-001",
        "TREASURY_SPEND",
        validProposal(),
        5,
        "submitter-node",
        govPolicyVersion,
        "hash-abc123"
    );
}

void testValidEnvelopeAccepted() {
    const auto env = validEnvelope();
    assert(env.isValid());
    assert(env.rejectionReason().empty());
    assert(env.governanceProposalId() == "gov-prop-001");
    assert(env.proposalType() == "TREASURY_SPEND");
    assert(env.submittedAtBlock() == 5);
    assert(env.submittedBy() == "submitter-node");
    assert(env.governancePolicyVersion() == "governance-v1");
    assert(env.summaryHash() == "hash-abc123");
    assert(env.treasuryProposal().proposalId() == "prop-001");
}

void testEmptyGovernanceProposalIdRejected() {
    const GovernanceProposalEnvelope env(
        "", "TREASURY_SPEND", validProposal(), 5,
        "submitter-node", "governance-v1", "hash-abc123"
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("governanceProposalId") != std::string::npos);
}

void testEmptyProposalTypeRejected() {
    const GovernanceProposalEnvelope env(
        "gov-prop-001", "", validProposal(), 5,
        "submitter-node", "governance-v1", "hash-abc123"
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("proposalType") != std::string::npos);
}

void testInvalidTreasuryProposalRejected() {
    const GovernanceProposalEnvelope env(
        "gov-prop-001", "TREASURY_SPEND",
        TreasuryProposal{},  // default-constructed = invalid
        5, "submitter-node", "governance-v1", "hash-abc123"
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("treasuryProposal") != std::string::npos);
}

void testEmptySubmittedByRejected() {
    const GovernanceProposalEnvelope env(
        "gov-prop-001", "TREASURY_SPEND", validProposal(), 5,
        "", "governance-v1", "hash-abc123"
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("submittedBy") != std::string::npos);
}

void testEmptyPolicyVersionRejected() {
    const GovernanceProposalEnvelope env(
        "gov-prop-001", "TREASURY_SPEND", validProposal(), 5,
        "submitter-node", "", "hash-abc123"
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("governancePolicyVersion") != std::string::npos);
}

void testEmptySummaryHashRejected() {
    const GovernanceProposalEnvelope env(
        "gov-prop-001", "TREASURY_SPEND", validProposal(), 5,
        "submitter-node", "governance-v1", ""
    );
    assert(!env.isValid());
    assert(env.rejectionReason().find("summaryHash") != std::string::npos);
}

void testPolicyVersionMismatchDetectedByBridge() {
    // When envelope.governancePolicyVersion != policy.policyVersion,
    // GovernanceApprovalBridge must reject it. This test verifies the
    // envelope itself accepts any non-empty policy version.
    const auto envV2 = validEnvelope("governance-v2");
    const GovernancePolicy policyV1("governance-v1", 0, 0, false, false);
    assert(envV2.isValid());
    assert(envV2.governancePolicyVersion() != policyV1.policyVersion());
}

void testDefaultConstructedRejected() {
    const GovernanceProposalEnvelope env;
    assert(!env.isValid());
}

void testSerializationNotEmpty() {
    const auto env = validEnvelope();
    const std::string s = env.serialize();
    assert(!s.empty());
    assert(s.find("gov-prop-001") != std::string::npos);
    assert(s.find("governance-v1") != std::string::npos);
}

} // namespace

int main() {
    testValidEnvelopeAccepted();
    testEmptyGovernanceProposalIdRejected();
    testEmptyProposalTypeRejected();
    testInvalidTreasuryProposalRejected();
    testEmptySubmittedByRejected();
    testEmptyPolicyVersionRejected();
    testEmptySummaryHashRejected();
    testPolicyVersionMismatchDetectedByBridge();
    testDefaultConstructedRejected();
    testSerializationNotEmpty();
    return 0;
}
