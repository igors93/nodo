#include "consensus/ValidatorPenaltyApplication.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace nodo::consensus;

namespace {

std::string hash64(char value) {
    return std::string(64, value);
}

SlashingEvidenceRecord evidence(
    char id,
    const std::string& validatorAddress,
    SlashingEvidenceType type,
    SlashingEvidenceSeverity severity
) {
    return SlashingEvidenceRecord(
        hash64(id),
        type,
        validatorAddress,
        hash64('f'),
        severity,
        1700000000 + id
    );
}

} // namespace

int main() {
    ValidatorPenaltyLedger ledger;
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeTestnetPolicy();

    const auto firstEvidence = evidence(
        'a',
        "validator-alpha",
        SlashingEvidenceType::DOUBLE_VOTE,
        SlashingEvidenceSeverity::SLASHABLE
    );

    const auto first = ledger.applyEvidence(firstEvidence, policy, 1800000000);
    assert(first.applied());
    assert(first.decision().has_value());
    assert(first.decision()->isValid());
    assert(first.decision()->action() == ValidatorPenaltyAction::SLASH);
    assert(first.decision()->slashAmountRawUnits() == 100000000);
    assert(ledger.containsEvidence(firstEvidence.evidenceId()));
    assert(ledger.size() == 1);
    assert(ledger.validatorIsJailed("validator-alpha"));
    assert(!ledger.validatorIsTombstoned("validator-alpha"));

    const auto roundTripped =
        ValidatorPenaltyDecision::deserialize(first.decision()->serialize());
    assert(roundTripped.penaltyId() == first.decision()->penaltyId());

    std::string tamperedPenaltyId = first.decision()->serialize();
    const std::size_t penaltyIdField = tamperedPenaltyId.find("penaltyId=");
    assert(penaltyIdField != std::string::npos);
    const std::size_t idStart =
        penaltyIdField + std::string("penaltyId=").size();
    tamperedPenaltyId[idStart] = tamperedPenaltyId[idStart] == '0' ? '1' : '0';

    bool rejectedTamperedPenaltyId = false;
    try {
        (void)ValidatorPenaltyDecision::deserialize(tamperedPenaltyId);
    } catch (const std::invalid_argument&) {
        rejectedTamperedPenaltyId = true;
    }
    assert(rejectedTamperedPenaltyId);

    const auto duplicate = ledger.applyEvidence(firstEvidence, policy, 1800000001);
    assert(duplicate.duplicate());
    assert(ledger.size() == 1);

    const auto duplicateDecision = ledger.applyDecision(*first.decision());
    assert(duplicateDecision.duplicate());
    assert(ledger.size() == 1);

    const auto secondEvidence = evidence(
        'b',
        "validator-alpha",
        SlashingEvidenceType::EQUIVOCATION,
        SlashingEvidenceSeverity::SLASHABLE
    );

    const auto second = ledger.applyEvidence(secondEvidence, policy, 1800000002);
    assert(second.applied());
    assert(second.decision()->action() == ValidatorPenaltyAction::TOMBSTONE);
    assert(ledger.size() == 2);
    assert(ledger.validatorIsTombstoned("validator-alpha"));
    assert(ledger.totalSlashAmountForValidator("validator-alpha") == 600000000);
    assert(ledger.decisionsForValidator("validator-alpha").size() == 2);
    assert(ledger.isValid());

    std::cout << "Validator penalty application tests passed.\n";
    return 0;
}
