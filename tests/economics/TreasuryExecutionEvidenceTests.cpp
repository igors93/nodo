#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendRecord;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::utils::Amount;

// ---- Test fixture helpers ----

TreasuryAccount validTreasury(
    Amount balance = Amount::fromRawUnits(1000000)
) {
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

TreasuryProposal validProposal(
    const std::string& proposalId = "prop-001"
) {
    return TreasuryProposal(
        proposalId, "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

TreasuryApproval validApproval(
    const std::string& proposalId = "prop-001"
) {
    return TreasuryApproval(
        "appr-001", proposalId, 3, "governance-node", "valid-proof-abc"
    );
}

// Build a matching spend record by running TreasurySpendValidator.
TreasurySpendRecord buildValidSpendRecord(
    std::uint64_t currentBlockHeight = 10,
    Amount balance = Amount::fromRawUnits(1000000)
) {
    const auto result = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(balance),
        validPolicy(),
        validProposal(),
        validApproval(),
        currentBlockHeight,
        Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    return result.spendRecord();
}

TreasuryExecutionEvidence validEvidence(
    std::uint64_t blockHeight = 10
) {
    const auto spendRecord = buildValidSpendRecord(blockHeight);
    return TreasuryExecutionEvidence(
        "ev-001",
        validProposal(),
        validApproval(),
        validPolicy(),
        validTreasury(),
        blockHeight,
        Amount::fromRawUnits(0),
        spendRecord,
        1900000001
    );
}

// ---- Tests ----

void testValidEvidencePasses() {
    const auto ev = validEvidence();
    assert(ev.isValid());
    assert(ev.rejectionReason().empty());
    assert(ev.evidenceId() == "ev-001");
    assert(ev.currentBlockHeight() == 10);
    assert(ev.createdAt() == 1900000001);
}

void testEmptyEvidenceIdRejected() {
    const auto spendRecord = buildValidSpendRecord();
    const TreasuryExecutionEvidence ev(
        "",
        validProposal(), validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("evidenceId") != std::string::npos);
}

void testInvalidProposalRejected() {
    const auto spendRecord = buildValidSpendRecord();
    const TreasuryExecutionEvidence ev(
        "ev-002",
        TreasuryProposal{},  // default-constructed = invalid
        validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("proposal") != std::string::npos);
}

void testInvalidApprovalRejected() {
    const auto spendRecord = buildValidSpendRecord();
    const TreasuryExecutionEvidence ev(
        "ev-003",
        validProposal(),
        TreasuryApproval{},  // default-constructed = invalid
        validPolicy(), validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("approval") != std::string::npos);
}

void testInvalidPolicyRejected() {
    const auto spendRecord = buildValidSpendRecord();
    const TreasuryExecutionEvidence ev(
        "ev-004",
        validProposal(), validApproval(),
        TreasuryPolicy{},  // default-constructed = invalid
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("policy") != std::string::npos);
}

void testInvalidTreasuryAccountRejected() {
    const auto spendRecord = buildValidSpendRecord();
    const TreasuryExecutionEvidence ev(
        "ev-005",
        validProposal(), validApproval(), validPolicy(),
        TreasuryAccount::invalid("test-invalid"),
        10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("treasuryAccountBefore") != std::string::npos);
}

void testSpendProposalAmountMismatchRejected() {
    // Create a spend record with a different amount than the proposal.
    auto spendRecord = buildValidSpendRecord();
    // Build a proposal with a different amount.
    TreasuryProposal mismatchedProposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(99999),  // different from 50000
        "fund validator", 1, 0, "proposer-node"
    );
    const TreasuryExecutionEvidence ev(
        "ev-006",
        mismatchedProposal, validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("amount") != std::string::npos);
}

void testSpendProposalRecipientMismatchRejected() {
    auto spendRecord = buildValidSpendRecord();
    TreasuryProposal mismatchedProposal(
        "prop-001", "wrong-recipient",  // different recipient
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
    const TreasuryExecutionEvidence ev(
        "ev-007",
        mismatchedProposal, validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("recipientAddress") != std::string::npos);
}

void testSpendProposalPurposeMismatchRejected() {
    auto spendRecord = buildValidSpendRecord();
    TreasuryProposal mismatchedProposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(50000),
        "different purpose",  // different purpose
        1, 0, "proposer-node"
    );
    const TreasuryExecutionEvidence ev(
        "ev-008",
        mismatchedProposal, validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("purpose") != std::string::npos);
}

void testSpendExecutionBlockMismatchRejected() {
    auto spendRecord = buildValidSpendRecord(10);
    // Evidence says currentBlockHeight=20, but spendRecord.executedAtBlock=10.
    const TreasuryExecutionEvidence ev(
        "ev-009",
        validProposal(), validApproval(), validPolicy(),
        validTreasury(), 20,  // different from executedAtBlock=10
        Amount::fromRawUnits(0), spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("executedAtBlock") != std::string::npos);
}

void testApprovalProposalMismatchRejected() {
    auto spendRecord = buildValidSpendRecord();
    TreasuryApproval wrongApproval(
        "appr-001", "wrong-proposal-id",  // approval for different proposal
        3, "governance-node", "valid-proof-abc"
    );
    const TreasuryExecutionEvidence ev(
        "ev-010",
        validProposal(), wrongApproval, validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendRecord, 1900000001
    );
    assert(!ev.isValid());
    assert(ev.rejectionReason().find("proposalId") != std::string::npos);
}

void testSerializationNotEmpty() {
    const auto ev = validEvidence();
    const std::string s = ev.serialize();
    assert(!s.empty());
    assert(s.find("ev-001") != std::string::npos);
    assert(s.find("prop-001") != std::string::npos);
}

} // namespace

int main() {
    testValidEvidencePasses();
    testEmptyEvidenceIdRejected();
    testInvalidProposalRejected();
    testInvalidApprovalRejected();
    testInvalidPolicyRejected();
    testInvalidTreasuryAccountRejected();
    testSpendProposalAmountMismatchRejected();
    testSpendProposalRecipientMismatchRejected();
    testSpendProposalPurposeMismatchRejected();
    testSpendExecutionBlockMismatchRejected();
    testApprovalProposalMismatchRejected();
    testSerializationNotEmpty();
    return 0;
}
