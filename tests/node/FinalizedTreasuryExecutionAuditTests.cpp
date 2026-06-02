#include "node/FinalizedTreasuryExecutionAudit.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/TreasuryExecutionEvidence.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::TreasuryExecutionEvidence;
using nodo::node::FinalizedTreasuryExecutionAudit;
using nodo::node::TreasuryExecutionAuditStatus;
using nodo::tests::fixtures::validExecutionEvidence;

TreasuryExecutionEvidence makeEvidence(
    const std::string& evidenceId,
    const std::string& lifecycleId,
    const std::string& proposalId,
    const std::string& governanceProposalId
) {
    return validExecutionEvidence(
        evidenceId,
        lifecycleId,
        proposalId,
        governanceProposalId
    );
}

void testEmptyEvidenceListAccepted() {
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence({});
    assert(result.accepted());
    assert(result.evidenceCount() == 0);
    assert(result.status() == TreasuryExecutionAuditStatus::ACCEPTED);
}

void testDistinctLifecycleBackedEvidenceAccepted() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001"),
        makeEvidence("ev-002", "lifecycle-002", "prop-002", "gov-prop-002")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(result.accepted());
    assert(result.evidenceCount() == 2);
}

void testDuplicateEvidenceIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-dup", "lifecycle-001", "prop-001", "gov-prop-001"),
        makeEvidence("ev-dup", "lifecycle-002", "prop-002", "gov-prop-002")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_EVIDENCE_ID);
}

void testDuplicateProposalIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "lifecycle-001", "prop-dup", "gov-prop-001"),
        makeEvidence("ev-002", "lifecycle-002", "prop-dup", "gov-prop-002")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_PROPOSAL_ID);
}

void testDuplicateLifecycleIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "lifecycle-dup", "prop-001", "gov-prop-001"),
        makeEvidence("ev-002", "lifecycle-dup", "prop-002", "gov-prop-002")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_LIFECYCLE_ID);
}

void testDuplicateGovernanceProposalIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-dup"),
        makeEvidence("ev-002", "lifecycle-002", "prop-002", "gov-prop-dup")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() ==
           TreasuryExecutionAuditStatus::DUPLICATE_GOVERNANCE_PROPOSAL_ID);
}

void testDuplicateApprovalIdRejected() {
    TreasuryExecutionEvidence first =
        makeEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001");
    TreasuryExecutionEvidence second = first;
    const std::vector<TreasuryExecutionEvidence> list = {
        first,
        second
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_EVIDENCE_ID);
}

void testInvalidEvidenceRejected() {
    const TreasuryExecutionEvidence invalidEv;
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence({invalidEv});
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::INVALID_EVIDENCE);
    assert(result.failedAtIndex() == 0);
}

void testStatusToString() {
    assert(nodo::node::treasuryExecutionAuditStatusToString(
               TreasuryExecutionAuditStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::node::treasuryExecutionAuditStatusToString(
               TreasuryExecutionAuditStatus::DUPLICATE_LIFECYCLE_ID) ==
           "DUPLICATE_LIFECYCLE_ID");
}

} // namespace

int main() {
    testEmptyEvidenceListAccepted();
    testDistinctLifecycleBackedEvidenceAccepted();
    testDuplicateEvidenceIdRejected();
    testDuplicateProposalIdRejected();
    testDuplicateLifecycleIdRejected();
    testDuplicateGovernanceProposalIdRejected();
    testDuplicateApprovalIdRejected();
    testInvalidEvidenceRejected();
    testStatusToString();
    return 0;
}
