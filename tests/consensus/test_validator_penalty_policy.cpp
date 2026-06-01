#include "consensus/ValidatorPenaltyApplication.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::consensus;

namespace {

std::string hash64(char value) {
    return std::string(64, value);
}

SlashingEvidenceRecord makeEvidence(
    const std::string& evidenceId,
    SlashingEvidenceType type,
    SlashingEvidenceSeverity severity
) {
    return SlashingEvidenceRecord(
        evidenceId,
        type,
        "validator-alpha",
        hash64('b'),
        severity,
        1700000000
    );
}

} // namespace

int main() {
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeTestnetPolicy();

    assert(policy.isValid());
    assert(policy.doubleVoteSlashRawUnits() == 100000000);
    assert(policy.equivocationSlashRawUnits() == 500000000);
    assert(policy.defaultJailEpochs() == 64);
    assert(policy.tombstoneEquivocation());

    const auto doubleVote = makeEvidence(
        hash64('a'),
        SlashingEvidenceType::DOUBLE_VOTE,
        SlashingEvidenceSeverity::SLASHABLE
    );

    assert(policy.actionForEvidence(doubleVote) == ValidatorPenaltyAction::SLASH);
    assert(policy.slashAmountForEvidence(doubleVote) == 100000000);
    assert(policy.jailEpochsForEvidence(doubleVote) == 64);

    const auto warning = makeEvidence(
        hash64('c'),
        SlashingEvidenceType::INVALID_SIGNATURE,
        SlashingEvidenceSeverity::WARNING
    );

    assert(policy.actionForEvidence(warning) == ValidatorPenaltyAction::WARNING);
    assert(policy.slashAmountForEvidence(warning) == 0);
    assert(policy.jailEpochsForEvidence(warning) == 0);

    const auto equivocation = makeEvidence(
        hash64('d'),
        SlashingEvidenceType::EQUIVOCATION,
        SlashingEvidenceSeverity::SLASHABLE
    );

    assert(policy.actionForEvidence(equivocation) == ValidatorPenaltyAction::TOMBSTONE);
    assert(policy.slashAmountForEvidence(equivocation) == 500000000);

    std::cout << "Validator penalty policy tests passed.\n";
    return 0;
}
