#include "node/FinalizedTreasuryExecutionAudit.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::node::FinalizedTreasuryExecutionAudit;
using nodo::node::TreasuryExecutionAuditStatus;
using nodo::utils::Amount;

TreasuryAccount validTreasury(Amount balance = Amount::fromRawUnits(1000000)) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr",
        balance, 0, false, ""
    );
}

TreasuryPolicy validPolicy() {
    return TreasuryPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000),
        Amount::fromRawUnits(100000),
        5, true, false
    );
}

TreasuryExecutionEvidence makeEvidence(
    const std::string& evidenceId,
    const std::string& proposalId,
    const std::string& approvalId
) {
    TreasuryProposal proposal(
        proposalId, "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
    TreasuryApproval approval(
        approvalId, proposalId, 3, "governance-node", "proof-" + approvalId
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), proposal, approval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());
    return TreasuryExecutionEvidence(
        evidenceId,
        proposal, approval, validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001
    );
}

void testEmptyEvidenceListAccepted() {
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence({});
    assert(result.accepted());
    assert(result.evidenceCount() == 0);
    assert(result.status() == TreasuryExecutionAuditStatus::ACCEPTED);
}

void testDistinctValidEvidenceAccepted() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "prop-001", "appr-001"),
        makeEvidence("ev-002", "prop-002", "appr-002")
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(result.accepted());
    assert(result.evidenceCount() == 2);
}

void testDuplicateEvidenceIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-dup", "prop-001", "appr-001"),
        makeEvidence("ev-dup", "prop-002", "appr-002")  // same evidenceId
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_EVIDENCE_ID);
    assert(result.reason().find("ev-dup") != std::string::npos);
}

void testDuplicateProposalIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "prop-dup", "appr-001"),
        makeEvidence("ev-002", "prop-dup", "appr-002")  // same proposalId
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_PROPOSAL_ID);
}

void testDuplicateApprovalIdRejected() {
    const std::vector<TreasuryExecutionEvidence> list = {
        makeEvidence("ev-001", "prop-001", "appr-dup"),
        makeEvidence("ev-002", "prop-002", "appr-dup")  // same approvalId
    };
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence(list);
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::DUPLICATE_APPROVAL_ID);
}

void testInvalidEvidenceRejected() {
    const TreasuryExecutionEvidence invalidEv;  // default-constructed = invalid
    const auto result = FinalizedTreasuryExecutionAudit::auditEvidence({invalidEv});
    assert(!result.accepted());
    assert(result.status() == TreasuryExecutionAuditStatus::INVALID_EVIDENCE);
    assert(result.failedAtIndex() == 0);
}

void testStatusToString() {
    assert(nodo::node::treasuryExecutionAuditStatusToString(
               TreasuryExecutionAuditStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::node::treasuryExecutionAuditStatusToString(
               TreasuryExecutionAuditStatus::DUPLICATE_PROPOSAL_ID) ==
           "DUPLICATE_PROPOSAL_ID");
    assert(nodo::node::treasuryExecutionAuditStatusToString(
               TreasuryExecutionAuditStatus::DUPLICATE_SPEND_ID) == "DUPLICATE_SPEND_ID");
}

} // namespace

int main() {
    testEmptyEvidenceListAccepted();
    testDistinctValidEvidenceAccepted();
    testDuplicateEvidenceIdRejected();
    testDuplicateProposalIdRejected();
    testDuplicateApprovalIdRejected();
    testInvalidEvidenceRejected();
    testStatusToString();
    return 0;
}
