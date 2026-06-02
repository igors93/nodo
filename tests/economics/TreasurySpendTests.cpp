#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryProposal.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendRecord;
using nodo::utils::Amount;

// ---------- TreasuryProposal ----------

void testValidProposal() {
    const TreasuryProposal p(
        "prop-001", "recipient-addr", Amount::fromRawUnits(5000),
        "fund community validator", 10, 1, "proposer-validator"
    );
    assert(p.isValid());
    assert(p.proposalId() == "prop-001");
    assert(p.amount() == Amount::fromRawUnits(5000));
}

void testProposalMissingRecipientRejected() {
    const TreasuryProposal p(
        "prop-002", "", Amount::fromRawUnits(5000),
        "purpose", 10, 1, "proposer"
    );
    assert(!p.isValid());
    assert(p.rejectionReason().find("recipientAddress") != std::string::npos);
}

void testProposalZeroAmountRejected() {
    const TreasuryProposal p(
        "prop-003", "recipient", Amount::fromRawUnits(0),
        "purpose", 10, 1, "proposer"
    );
    assert(!p.isValid());
    assert(p.rejectionReason().find("amount") != std::string::npos);
}

// ---------- TreasuryApproval ----------

void testValidApproval() {
    const TreasuryApproval a(
        "appr-001", "prop-001", 15,
        "governance-node", "proof-hash-abc123"
    );
    assert(a.isValid());
    assert(a.approvalId() == "appr-001");
    assert(a.proposalId() == "prop-001");
    assert(a.approvalProof() == "proof-hash-abc123");
}

void testApprovalMissingProofRejected() {
    const TreasuryApproval a(
        "appr-002", "prop-001", 15, "governance-node", ""
    );
    assert(!a.isValid());
    assert(a.rejectionReason().find("approvalProof") != std::string::npos);
}

// ---------- TreasurySpendRecord ----------

void testValidSpendRecord() {
    const TreasurySpendRecord r(
        "spend-001", "prop-001", "recipient-addr",
        Amount::fromRawUnits(500),
        "fund validator", 20, 1,
        Amount::fromRawUnits(10000),
        Amount::fromRawUnits(9500)
    );
    assert(r.isValid());
    assert(r.spendId() == "spend-001");
    assert(r.treasuryBalanceAfter() == Amount::fromRawUnits(9500));
}

void testSpendRecordBalanceMismatchRejected() {
    const TreasurySpendRecord r(
        "spend-002", "prop-001", "recipient",
        Amount::fromRawUnits(500),
        "purpose", 20, 1,
        Amount::fromRawUnits(10000),
        Amount::fromRawUnits(9999)  // wrong: should be 9500
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("balance") != std::string::npos ||
           r.rejectionReason().find("mismatch") != std::string::npos);
}

// When balanceBefore < amount, the computed ending is negative.
// Since Amount prevents negative construction, we pass a non-matching
// balanceAfter (the arithmetic check detects the inconsistency).
void testSpendRecordNegativeBalanceRejectedViaArithmetic() {
    // balanceBefore=100, amount=200 -> computed = -100 != balanceAfter=0
    const TreasurySpendRecord r(
        "spend-003", "prop-001", "recipient",
        Amount::fromRawUnits(200),
        "purpose", 20, 1,
        Amount::fromRawUnits(100),
        Amount::fromRawUnits(0)   // wrong: -100 != 0
    );
    assert(!r.isValid());
    assert(r.rejectionReason().find("balance") != std::string::npos ||
           r.rejectionReason().find("mismatch") != std::string::npos);
}

} // namespace

int main() {
    testValidProposal();
    testProposalMissingRecipientRejected();
    testProposalZeroAmountRejected();
    testValidApproval();
    testApprovalMissingProofRejected();
    testValidSpendRecord();
    testSpendRecordBalanceMismatchRejected();
    testSpendRecordNegativeBalanceRejectedViaArithmetic();
    return 0;
}
