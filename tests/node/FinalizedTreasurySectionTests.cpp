#include "node/FinalizedTreasurySection.hpp"
#include "node/FinalizedTreasurySectionCodec.hpp"
#include "node/FinalizedTreasurySectionValidator.hpp"
#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "economics/TreasurySpendValidator.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::GovernanceApprovalBridge;
using nodo::economics::GovernanceApprovalContext;
using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernancePolicy;
using nodo::economics::GovernanceProposalEnvelope;
using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendRecord;
using nodo::node::FinalizedTreasurySection;
using nodo::node::FinalizedTreasurySectionCodec;
using nodo::node::FinalizedTreasurySectionValidator;
using nodo::node::TreasurySectionValidationStatus;
using nodo::utils::Amount;

TreasurySpendRecord validRecord(
    const std::string& id,
    std::int64_t amount,
    std::int64_t balanceBefore
) {
    return TreasurySpendRecord(
        id, "prop-" + id, "recipient-addr",
        Amount::fromRawUnits(amount),
        "fund validator", 10, 0,
        Amount::fromRawUnits(balanceBefore),
        Amount::fromRawUnits(balanceBefore - amount)
    );
}

TreasuryAccount validTreasury(Amount balance = Amount::fromRawUnits(1000000)) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr", balance, 0, false, ""
    );
}

TreasuryPolicy validSpendPolicy() {
    return TreasuryPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000), Amount::fromRawUnits(100000),
        5, true, false
    );
}

// Build governance-backed evidence using the canonical bridge path.
TreasuryExecutionEvidence makeEvidence(
    const std::string& evidenceId,
    const std::string& proposalId,
    std::int64_t amount,
    std::int64_t balance = 1000000,
    std::uint64_t blockHeight = 10
) {
    TreasuryProposal proposal(
        proposalId, "recipient-addr",
        Amount::fromRawUnits(amount),
        "fund validator", 1, 0, "proposer-node"
    );
    const auto treasury = validTreasury(Amount::fromRawUnits(balance));
    const auto spendPolicy = validSpendPolicy();

    // Governance objects — unique governance proposal per treasury proposal.
    const std::string govPropId = "gov-" + proposalId;
    const GovernancePolicy govPolicy("governance-v1", 10, 5, true, false);
    const GovernanceProposalEnvelope envelope(
        govPropId, "TREASURY_SPEND",
        proposal, 5, "submitter-node", "governance-v1", "hash-" + proposalId
    );
    const GovernanceDecisionRecord decision(
        "dec-" + proposalId, govPropId, "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "proof-" + proposalId, "governance-v1"
    );

    const auto bridgeResult = GovernanceApprovalBridge::produceTreasuryApproval(
        govPolicy, envelope, decision
    );
    assert(bridgeResult.isAccepted());
    const TreasuryApproval& approval = bridgeResult.treasuryApproval();

    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        treasury, spendPolicy, proposal, approval,
        blockHeight, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext ctx;
    ctx.governancePolicy = govPolicy;
    ctx.governanceProposalEnvelope = envelope;
    ctx.governanceDecisionRecord = decision;

    return TreasuryExecutionEvidence(
        evidenceId, proposal, approval, spendPolicy,
        treasury, blockHeight, Amount::fromRawUnits(0),
        spendResult.spendRecord(), 1900000001,
        std::move(ctx)
    );
}

// ---- Section construction tests ----

void testEmptySectionIsValid() {
    const FinalizedTreasurySection section;
    assert(section.isValid());
    assert(section.spendRecordCount() == 0);
    assert(section.evidenceCount() == 0);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(!section.hasEvidence());
}

void testSectionWithOneRecordIsValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));
    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 1);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(5000));
    assert(!section.hasEvidence());
}

void testSectionWithMultipleRecordsIsValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 3000, 100000));
    records.push_back(validRecord("spend-002", 7000, 97000));
    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 2);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(10000));
}

void testSectionWithInvalidRecordRejected() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));
    records.emplace_back();  // default-constructed — invalid
    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());
    assert(section.rejectionReason().find("invalid") != std::string::npos);
}

void testSectionWithEvidenceIsValid() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        makeEvidence("ev-001", "prop-001", 50000)
    };
    const FinalizedTreasurySection section(std::move(evidence));
    assert(section.isValid());
    assert(section.evidenceCount() == 1);
    assert(section.spendRecordCount() == 1);
    assert(section.hasEvidence());
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(50000));
}

void testSectionWithMultipleEvidenceIsValid() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        makeEvidence("ev-001", "prop-001", 30000),
        makeEvidence("ev-002", "prop-002", 20000, 970000)
    };
    const FinalizedTreasurySection section(std::move(evidence));
    assert(section.isValid());
    assert(section.evidenceCount() == 2);
    assert(section.spendRecordCount() == 2);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(50000));
}

// ---- Validator tests ----

void testValidatorPassesForEmptySection() {
    const FinalizedTreasurySection section;
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(result.passed());
    assert(result.status() == TreasurySectionValidationStatus::VALID);
}

// Non-empty section with only spend records (no evidence) is rejected in production.
void testValidatorRejectsSpendWithoutEvidence() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 1000, 50000));
    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());  // section itself is structurally valid
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::SPEND_WITHOUT_EVIDENCE);
    assert(!result.reason().empty());
}

// Governance-backed evidence is accepted by the validator.
void testValidatorPassesForEvidenceSection() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        makeEvidence("ev-001", "prop-001", 50000)
    };
    const FinalizedTreasurySection section(std::move(evidence));
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(result.passed());
    assert(result.status() == TreasurySectionValidationStatus::VALID);
}

// Evidence without governance context is rejected by the validator.
void testValidatorRejectsEvidenceWithoutGovernanceContext() {
    TreasuryProposal proposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
    const TreasuryApproval directApproval(
        "appr-001", "prop-001", 3, "governance-node", "manual-proof"
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validSpendPolicy(), proposal, directApproval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    std::vector<TreasuryExecutionEvidence> evidence = {
        TreasuryExecutionEvidence(
            "ev-no-gov",
            proposal, directApproval, validSpendPolicy(),
            validTreasury(), 10, Amount::fromRawUnits(0),
            spendResult.spendRecord(), 1900000001
            // no governance context
        )
    };
    const FinalizedTreasurySection section(std::move(evidence));
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::MISSING_GOVERNANCE_CONTEXT);
}

void testValidatorFailsForInvalidSection() {
    std::vector<TreasurySpendRecord> records;
    records.emplace_back();  // invalid
    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::INVALID_SECTION);
}

// ---- Codec tests ----

void testCodecRoundTripsEmptySection() {
    const FinalizedTreasurySection original;
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 0);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(0));
}

void testCodecRoundTripsLegacySection() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 4000, 100000));
    records.push_back(validRecord("spend-002", 6000, 96000));
    const FinalizedTreasurySection original(std::move(records));
    assert(original.isValid());
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 2);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(10000));
    assert(decoded.spendRecords()[0].spendId() == "spend-001");
}

} // namespace

int main() {
    testEmptySectionIsValid();
    testSectionWithOneRecordIsValid();
    testSectionWithMultipleRecordsIsValid();
    testSectionWithInvalidRecordRejected();
    testSectionWithEvidenceIsValid();
    testSectionWithMultipleEvidenceIsValid();
    testValidatorPassesForEmptySection();
    testValidatorRejectsSpendWithoutEvidence();
    testValidatorPassesForEvidenceSection();
    testValidatorRejectsEvidenceWithoutGovernanceContext();
    testValidatorFailsForInvalidSection();
    testCodecRoundTripsEmptySection();
    testCodecRoundTripsLegacySection();
    return 0;
}
