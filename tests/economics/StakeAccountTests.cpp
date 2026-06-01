#include "economics/StakeAccount.hpp"
#include "economics/StakeSlashApplication.hpp"
#include "economics/SupplyAudit.hpp"
#include "economics/ValidatorStakeState.hpp"
#include "consensus/SlashingEvidence.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

void testFreshAccountIsEligible() {
    const nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(acct.isEligible());
    assert(!acct.jailed());
    assert(!acct.tombstoned());
    assert(acct.bondedAmount() == nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.slashedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(acct.isValid());
}

void testJailBlocksEligibility() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.jail();
    assert(!acct.isEligible());
    assert(acct.jailed());
    assert(!acct.tombstoned());
}

void testUnjailRestoresEligibility() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.jail();
    acct.unjail();
    assert(acct.isEligible());
}

void testTombstoneBlocksEligibilityPermanently() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.tombstone();
    assert(!acct.isEligible());
    assert(acct.tombstoned());
    acct.unjail(); // unjail must not override tombstone
    assert(!acct.isEligible());
}

void testSlashReducesEffectiveStake() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.applySlash(nodo::utils::Amount::fromRawUnits(300));
    assert(acct.slashedAmount() == nodo::utils::Amount::fromRawUnits(300));
    assert(acct.bondedAmount() == nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.isValid());
}

void testFullSlashAutoTombstones() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.applySlash(nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.tombstoned());
    assert(!acct.isEligible());
}

void testSlashBeyondStakeThrows() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(100)
    );
    bool threw = false;
    try {
        acct.applySlash(nodo::utils::Amount::fromRawUnits(200));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testSlashApplicationIdempotency() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount("validator-a", nodo::utils::Amount::fromRawUnits(1000))
    );

    const auto r1 = nodo::economics::StakeSlashApplication::apply(
        state,
        "evidence-001",
        nodo::utils::Amount::fromRawUnits(100)
    );
    const auto r2 = nodo::economics::StakeSlashApplication::apply(
        state,
        "evidence-001",
        nodo::utils::Amount::fromRawUnits(100)
    );

    assert(r1 == nodo::economics::SlashResult::APPLIED);
    assert(r2 == nodo::economics::SlashResult::ALREADY_APPLIED);
    // Stake must only have been reduced once.
    assert(state.account().slashedAmount() == nodo::utils::Amount::fromRawUnits(100));
}

void testSlashWithEmptyEvidenceIdRejected() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount("validator-a", nodo::utils::Amount::fromRawUnits(1000))
    );
    const auto r = nodo::economics::StakeSlashApplication::apply(
        state,
        "",
        nodo::utils::Amount::fromRawUnits(100)
    );
    assert(r == nodo::economics::SlashResult::INVALID_EVIDENCE);
}

void testSlashTombstonedValidatorRejected() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount("validator-a", nodo::utils::Amount::fromRawUnits(1000))
    );
    state.account().tombstone();

    const auto r = nodo::economics::StakeSlashApplication::apply(
        state,
        "evidence-002",
        nodo::utils::Amount::fromRawUnits(100)
    );
    assert(r == nodo::economics::SlashResult::VALIDATOR_TOMBSTONED);
}

nodo::consensus::SlashingEvidenceRecord evidenceRecord(
    const std::string& evidenceId,
    nodo::consensus::SlashingEvidenceType type
) {
    return nodo::consensus::SlashingEvidenceRecord(
        evidenceId,
        type,
        "validator-a",
        "payload-hash-" + evidenceId,
        nodo::consensus::SlashingEvidenceSeverity::SLASHABLE,
        1000
    );
}

void testPenaltyDecisionAppliesSlashAndIsIdempotent() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(1000)
        )
    );

    const auto decision =
        nodo::consensus::ValidatorPenaltyDecision::create(
            evidenceRecord(
                "penalty-evidence-001",
                nodo::consensus::SlashingEvidenceType::DOUBLE_VOTE
            ),
            nodo::consensus::ValidatorPenaltyPolicy(
                100,
                100,
                64,
                false
            ),
            1001
        );

    const auto r1 =
        nodo::economics::StakeSlashApplication::applyPenaltyDecision(
            state,
            decision
        );

    const auto r2 =
        nodo::economics::StakeSlashApplication::applyPenaltyDecision(
            state,
            decision
        );

    assert(r1 == nodo::economics::SlashResult::APPLIED);
    assert(r2 == nodo::economics::SlashResult::ALREADY_APPLIED);
    assert(state.account().slashedAmount() == nodo::utils::Amount::fromRawUnits(100));
    assert(!state.account().isEligible());
    assert(state.account().jailed());
}

void testPenaltyDecisionNeverMakesStakeNegative() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(50)
        )
    );

    const auto decision =
        nodo::consensus::ValidatorPenaltyDecision::create(
            evidenceRecord(
                "penalty-evidence-002",
                nodo::consensus::SlashingEvidenceType::DOUBLE_VOTE
            ),
            nodo::consensus::ValidatorPenaltyPolicy(
                100,
                100,
                64,
                false
            ),
            1002
        );

    const auto result =
        nodo::economics::StakeSlashApplication::applyPenaltyDecision(
            state,
            decision
        );

    assert(result == nodo::economics::SlashResult::WOULD_EXCEED_STAKE);
    assert(state.account().slashedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(state.account().bondedAmount() == nodo::utils::Amount::fromRawUnits(50));
    assert(state.account().isValid());
}

void testPenaltyDecisionJailAndTombstoneAffectEligibility() {
    nodo::economics::ValidatorStakeState jailedState(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(1000)
        )
    );

    const auto jailDecision =
        nodo::consensus::ValidatorPenaltyDecision::create(
            evidenceRecord(
                "penalty-evidence-003",
                nodo::consensus::SlashingEvidenceType::INVALID_SIGNATURE
            ),
            nodo::consensus::ValidatorPenaltyPolicy(
                100,
                100,
                64,
                false
            ),
            1003
        );

    assert(nodo::economics::StakeSlashApplication::applyPenaltyDecision(
               jailedState,
               jailDecision
           ) == nodo::economics::SlashResult::APPLIED);
    assert(jailedState.account().jailed());
    assert(!jailedState.account().isEligible());

    nodo::economics::ValidatorStakeState tombstonedState(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(1000)
        )
    );

    const auto tombstoneDecision =
        nodo::consensus::ValidatorPenaltyDecision::create(
            evidenceRecord(
                "penalty-evidence-004",
                nodo::consensus::SlashingEvidenceType::EQUIVOCATION
            ),
            nodo::consensus::ValidatorPenaltyPolicy(
                100,
                100,
                64,
                true
            ),
            1004
        );

    assert(nodo::economics::StakeSlashApplication::applyPenaltyDecision(
               tombstonedState,
               tombstoneDecision
           ) == nodo::economics::SlashResult::APPLIED);
    assert(tombstonedState.account().tombstoned());
    assert(!tombstonedState.account().isEligible());
}

void testSupplyAuditIncludesPenaltySlash() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(1000)
        )
    );

    const auto decision =
        nodo::consensus::ValidatorPenaltyDecision::create(
            evidenceRecord(
                "penalty-evidence-005",
                nodo::consensus::SlashingEvidenceType::DOUBLE_VOTE
            ),
            nodo::consensus::ValidatorPenaltyPolicy(
                100,
                100,
                64,
                false
            ),
            1005
        );

    assert(nodo::economics::StakeSlashApplication::applyPenaltyDecision(
               state,
               decision
           ) == nodo::economics::SlashResult::APPLIED);

    const auto audit =
        nodo::economics::SupplyAudit::audit(
            nodo::utils::Amount::fromRawUnits(1100),
            {state},
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0)
        );

    assert(audit.isValid());
    assert(audit.totalSlashed() == nodo::utils::Amount::fromRawUnits(100));
}

void testAccountSerializes() {
    const nodo::economics::StakeAccount acct(
        "validator-x",
        nodo::utils::Amount::fromRawUnits(5000)
    );
    const std::string s = acct.serialize();
    assert(!s.empty());
    assert(s.find("validator-x") != std::string::npos);
    assert(s.find("5000") != std::string::npos);
}

} // namespace

int main() {
    testFreshAccountIsEligible();
    testJailBlocksEligibility();
    testUnjailRestoresEligibility();
    testTombstoneBlocksEligibilityPermanently();
    testSlashReducesEffectiveStake();
    testFullSlashAutoTombstones();
    testSlashBeyondStakeThrows();
    testSlashApplicationIdempotency();
    testSlashWithEmptyEvidenceIdRejected();
    testSlashTombstonedValidatorRejected();
    testPenaltyDecisionAppliesSlashAndIsIdempotent();
    testPenaltyDecisionNeverMakesStakeNegative();
    testPenaltyDecisionJailAndTombstoneAffectEligibility();
    testSupplyAuditIncludesPenaltySlash();
    testAccountSerializes();
    return 0;
}
