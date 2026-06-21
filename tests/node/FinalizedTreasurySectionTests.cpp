#include "node/FinalizedTreasurySection.hpp"
#include "node/FinalizedTreasurySectionCodec.hpp"
#include "node/FinalizedTreasurySectionValidator.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/DefenseModeState.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "economics/TreasurySpendValidator.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasurySpendRecord;
using nodo::node::FinalizedTreasurySection;
using nodo::node::FinalizedTreasurySectionCodec;
using nodo::node::FinalizedTreasurySectionValidator;
using nodo::node::TreasurySectionValidationStatus;
using nodo::tests::fixtures::validExecutionEvidence;
using nodo::tests::fixtures::validSpendPolicy;
using nodo::tests::fixtures::validTreasury;
using nodo::tests::fixtures::validTreasuryProposal;
using nodo::utils::Amount;

TreasurySpendRecord validRecord(
    const std::string& id,
    std::int64_t amount,
    std::int64_t balanceBefore
) {
    return TreasurySpendRecord(
        id,
        "prop-" + id,
        "recipient-addr",
        Amount::fromRawUnits(amount),
        "fund-validator",
        10,
        0,
        Amount::fromRawUnits(balanceBefore),
        Amount::fromRawUnits(balanceBefore - amount)
    );
}

void testEmptySectionIsValid() {
    const FinalizedTreasurySection section;
    assert(section.isValid());
    assert(section.spendRecordCount() == 0);
    assert(section.evidenceCount() == 0);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(!section.hasEvidence());
}

void testSectionWithOneRecordIsStructurallyValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));
    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 1);
    assert(!section.hasEvidence());
}

void testSectionWithInvalidRecordRejected() {
    std::vector<TreasurySpendRecord> records;
    records.emplace_back();
    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());
}

void testSectionWithLifecycleEvidenceIsValid() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        validExecutionEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001")
    };
    const FinalizedTreasurySection section(std::move(evidence));
    assert(section.isValid());
    assert(section.evidenceCount() == 1);
    assert(section.spendRecordCount() == 1);
    assert(section.hasEvidence());
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(50000));
}

void testSectionWithMultipleLifecycleEvidenceIsValid() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        validExecutionEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001"),
        validExecutionEvidence("ev-002", "lifecycle-002", "prop-002", "gov-prop-002")
    };
    const FinalizedTreasurySection section(std::move(evidence));
    assert(section.isValid());
    assert(section.evidenceCount() == 2);
    assert(section.spendRecordCount() == 2);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(100000));
}

void testValidatorPassesForEmptySection() {
    const FinalizedTreasurySection section;
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(result.passed());
    assert(result.status() == TreasurySectionValidationStatus::VALID);
}

void testValidatorRejectsSpendWithoutEvidence() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 1000, 50000));
    const FinalizedTreasurySection section(std::move(records));
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::SPEND_WITHOUT_EVIDENCE);
}

void testValidatorPassesForLifecycleEvidenceSection() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        validExecutionEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001")
    };
    const FinalizedTreasurySection section(std::move(evidence));
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(result.passed());
    assert(result.status() == TreasurySectionValidationStatus::VALID);
}

void testValidatorRejectsEvidenceWithoutGovernanceContext() {
    const auto proposal = validTreasuryProposal();
    const TreasuryApproval directApproval(
        "appr-001",
        proposal.proposalId(),
        3,
        "governance-node",
        "manual-proof"
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        nodo::economics::DefenseModeState::INACTIVE,
        nodo::economics::DefenseModePolicy::defaultPolicy(),
        validTreasury(),
        validSpendPolicy(),
        proposal,
        directApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    std::vector<TreasuryExecutionEvidence> evidence = {
        TreasuryExecutionEvidence(
            "ev-no-gov",
            proposal,
            directApproval,
            validSpendPolicy(),
            validTreasury(),
            10,
            Amount::fromRawUnits(0),
            spendResult.spendRecord(),
            1900000001
        )
    };
    const FinalizedTreasurySection section(std::move(evidence));
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::MISSING_GOVERNANCE_CONTEXT);
}

void testCodecRoundTripsEmptySection() {
    const FinalizedTreasurySection original;
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 0);
}

void testCodecRoundTripsLegacySection() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 4000, 100000));
    const FinalizedTreasurySection original(std::move(records));
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 1);
    assert(decoded.spendRecords()[0].spendId() == "spend-001");
}

void testCodecRoundTripsLifecycleEvidenceSection() {
    std::vector<TreasuryExecutionEvidence> evidence = {
        validExecutionEvidence("ev-001", "lifecycle-001", "prop-001", "gov-prop-001")
    };
    const FinalizedTreasurySection original(std::move(evidence));
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.hasEvidence());
    assert(decoded.executionEvidence()[0].hasGovernanceContext());

    const auto validation = FinalizedTreasurySectionValidator::validate(decoded);
    assert(validation.passed());
}

void testCodecRejectsPartialNumericCount() {
    const FinalizedTreasurySection section;
    std::string encoded = FinalizedTreasurySectionCodec::encode(section);
    const std::string target = "spendRecordCount=0\n";
    const std::string replacement = "spendRecordCount=0abc\n";
    const std::size_t pos = encoded.find(target);
    assert(pos != std::string::npos);
    encoded.replace(pos, target.size(), replacement);

    bool threw = false;
    try {
        (void)FinalizedTreasurySectionCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testCodecRejectsPartialNumericSpendAmount() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 4000, 100000));
    const FinalizedTreasurySection section(std::move(records));
    std::string encoded = FinalizedTreasurySectionCodec::encode(section);
    const std::string target = "spend.0.amountRawUnits=4000\n";
    const std::string replacement =
        "spend.0.amountRawUnits=4000abc\n";
    const std::size_t pos = encoded.find(target);
    assert(pos != std::string::npos);
    encoded.replace(pos, target.size(), replacement);

    bool threw = false;
    try {
        (void)FinalizedTreasurySectionCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    testEmptySectionIsValid();
    testSectionWithOneRecordIsStructurallyValid();
    testSectionWithInvalidRecordRejected();
    testSectionWithLifecycleEvidenceIsValid();
    testSectionWithMultipleLifecycleEvidenceIsValid();
    testValidatorPassesForEmptySection();
    testValidatorRejectsSpendWithoutEvidence();
    testValidatorPassesForLifecycleEvidenceSection();
    testValidatorRejectsEvidenceWithoutGovernanceContext();
    testCodecRoundTripsEmptySection();
    testCodecRoundTripsLegacySection();
    testCodecRoundTripsLifecycleEvidenceSection();
    testCodecRejectsPartialNumericCount();
    testCodecRejectsPartialNumericSpendAmount();
    return 0;
}
