#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::core::ValidatorRegistrationStatus;
using nodo::core::ValidatorRegistryUpdateStatus;
using nodo::crypto::KeyPair;

constexpr std::int64_t kTs = 1900000000;

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

ValidatorRegistrationRecord makeRecord(
    const std::string& seed,
    std::uint64_t activationEpoch = 1
) {
    const auto kp = KeyPair::createDeterministicBls12381KeyPair(seed);
    return ValidatorRegistrationRecord(
        nodo::crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value(),
        kp.publicKey(),
        activationEpoch,
        "meta-" + seed,
        kTs
    );
}

// Helper: register and immediately get ACTIVE entry (genesis/init path).
std::string registerActive(ValidatorRegistry& reg, const std::string& seed) {
    const auto rec = makeRecord(seed, 1);
    const auto result = reg.registerValidator(rec);
    requireCondition(result.accepted(), "registerValidator failed for seed: " + seed);
    return rec.validatorAddress();
}

// ── 1. Register creates an entry (existing genesis behavior = ACTIVE) ─────────

void testRegisterCreatesEntry() {
    ValidatorRegistry registry;
    const auto rec = makeRecord("lifecycle-p2-a", 1);
    const auto result = registry.registerValidator(rec);

    requireCondition(result.accepted(), "registerValidator should be accepted");
    requireCondition(registry.size() == 1U, "Registry must have one entry");
    requireCondition(
        registry.isEligibleForConsensus(rec.validatorAddress()),
        "Genesis-registered validator must be immediately eligible"
    );
}

// ── 2. pendingValidatorAddresses / eligibleValidatorAddresses are distinct ────

void testPendingAndEligibleAreSeparate() {
    ValidatorRegistry registry;
    const auto activeAddr = registerActive(registry, "lifecycle-p2-b-active");

    requireCondition(
        registry.eligibleValidatorAddresses().size() == 1U,
        "One eligible validator expected"
    );
    requireCondition(
        registry.pendingValidatorAddresses().empty(),
        "No pending validators expected after direct registration"
    );
}

// ── 3. activateValidator on already-ACTIVE returns ALREADY_ACTIVE ─────────────

void testActivateAlreadyActiveReturnsAlreadyActive() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-c");

    const auto result = registry.activateValidator(addr, 1, kTs + 1);
    requireCondition(
        result.status() == ValidatorRegistryUpdateStatus::ALREADY_ACTIVE,
        "activateValidator on ACTIVE must return ALREADY_ACTIVE"
    );
}

// ── 4. Jail ACTIVE validator ──────────────────────────────────────────────────

void testJailActiveValidator() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-d");

    const auto jailResult = registry.jailValidator(addr, 5, kTs + 1);
    requireCondition(
        jailResult.status() == ValidatorRegistryUpdateStatus::JAILED,
        "jailValidator on ACTIVE must succeed"
    );
    requireCondition(
        jailResult.entry().status() == ValidatorRegistrationStatus::JAILED,
        "Entry must be JAILED after jail"
    );
    requireCondition(
        jailResult.entry().jailUntilEpoch() == 5,
        "jailUntilEpoch must be stored"
    );
    requireCondition(
        registry.eligibleValidatorAddresses().empty(),
        "Jailed validator must not be eligible for consensus"
    );
    requireCondition(
        registry.jailedValidatorAddresses().size() == 1U,
        "Jailed validator must appear in jailedValidatorAddresses"
    );
}

// ── 5. Cannot jail already-jailed ────────────────────────────────────────────

void testJailAlreadyJailedRejected() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-e");
    registry.jailValidator(addr, 5, kTs + 1);

    const auto doubleJail = registry.jailValidator(addr, 10, kTs + 2);
    requireCondition(
        doubleJail.status() == ValidatorRegistryUpdateStatus::ALREADY_JAILED,
        "Jailing an already-jailed validator must return ALREADY_JAILED"
    );
}

// ── 6. Unjail before epoch rejected ──────────────────────────────────────────

void testUnjailBeforeEpochRejected() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-f");
    registry.jailValidator(addr, 10, kTs + 1);

    // currentEpoch=5 < jailUntilEpoch=10
    const auto tooEarly = registry.unjailValidator(addr, 5, kTs + 2);
    requireCondition(
        tooEarly.status() == ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
        "Unjail before jailUntilEpoch must be rejected"
    );
    requireCondition(
        registry.jailedValidatorAddresses().size() == 1U,
        "Validator must still be jailed"
    );
}

// ── 7. Unjail at correct epoch ────────────────────────────────────────────────

void testUnjailAtEpoch() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-g");
    registry.jailValidator(addr, 3, kTs + 1);

    const auto unjailResult = registry.unjailValidator(addr, 3, kTs + 3);
    requireCondition(
        unjailResult.status() == ValidatorRegistryUpdateStatus::UNJAILED,
        "Unjail at jailUntilEpoch must succeed"
    );
    requireCondition(
        unjailResult.entry().status() == ValidatorRegistrationStatus::ACTIVE,
        "Entry must be ACTIVE after unjail"
    );
    requireCondition(
        registry.eligibleValidatorAddresses().size() == 1U,
        "Unjailed validator must be eligible for consensus again"
    );
}

// ── 8. Exit request from ACTIVE ───────────────────────────────────────────────

void testExitRequestFromActive() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-h");

    const auto exitResult = registry.requestExit(addr, 100, kTs + 1);
    requireCondition(
        exitResult.status() == ValidatorRegistryUpdateStatus::EXIT_REQUESTED,
        "requestExit from ACTIVE must succeed"
    );
    requireCondition(
        exitResult.entry().status() == ValidatorRegistrationStatus::EXIT_REQUESTED,
        "Entry must be EXIT_REQUESTED"
    );
    requireCondition(
        exitResult.entry().exitRequestHeight() == 100,
        "exitRequestHeight must be stored"
    );
    requireCondition(
        registry.eligibleValidatorAddresses().empty(),
        "EXIT_REQUESTED validator must not be eligible for consensus"
    );
    requireCondition(
        registry.exitRequestedValidatorAddresses().size() == 1U,
        "EXIT_REQUESTED validator must appear in exitRequestedValidatorAddresses"
    );
}

// ── 9. Exit request from JAILED ───────────────────────────────────────────────

void testExitRequestFromJailed() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-i");
    registry.jailValidator(addr, 5, kTs + 1);

    const auto exitResult = registry.requestExit(addr, 200, kTs + 2);
    requireCondition(
        exitResult.status() == ValidatorRegistryUpdateStatus::EXIT_REQUESTED,
        "requestExit from JAILED must succeed"
    );
}

// ── 10. Complete exit ──────────────────────────────────────────────────────────

void testCompleteExit() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-j");
    registry.requestExit(addr, 100, kTs + 1);

    const auto completeResult = registry.completeExit(addr, kTs + 2);
    requireCondition(
        completeResult.deactivated(),
        "completeExit must return a deactivated result"
    );
    requireCondition(
        completeResult.entry().status() == ValidatorRegistrationStatus::EXITED,
        "Entry must be EXITED after completeExit"
    );
    requireCondition(
        registry.exitRequestedValidatorAddresses().empty(),
        "No EXIT_REQUESTED addresses after completeExit"
    );
}

// ── 11. Cannot complete exit without prior exit request ───────────────────────

void testCompleteExitWithoutRequest() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-k");

    const auto badResult = registry.completeExit(addr, kTs + 1);
    requireCondition(
        badResult.status() == ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
        "completeExit on ACTIVE must be rejected"
    );
}

// ── 12. Stake update ──────────────────────────────────────────────────────────

void testUpdateStake() {
    ValidatorRegistry registry;
    const auto rec = makeRecord("lifecycle-p2-l", 1);
    registry.registerValidator(rec);

    const auto result = registry.updateStake(rec.validatorAddress(), 5'000'000, kTs + 1);
    requireCondition(result.success(), "updateStake must succeed");
    requireCondition(
        result.entry().stakeAmount() == 5'000'000,
        "stakeAmount must be updated"
    );
}

// ── 13. Deactivate validator ──────────────────────────────────────────────────

void testDeactivateValidator() {
    ValidatorRegistry registry;
    const auto addr = registerActive(registry, "lifecycle-p2-m");

    const auto deactResult = registry.deactivateValidator(addr, kTs + 1);
    requireCondition(
        deactResult.deactivated(),
        "deactivateValidator must return deactivated result"
    );
    requireCondition(
        registry.eligibleValidatorAddresses().empty(),
        "Deactivated validator must not be eligible"
    );
}

// ── 14. isEligibleForConsensus predicate ──────────────────────────────────────

void testIsEligibleForConsensus() {
    ValidatorRegistry registry;
    const auto activeAddr = registerActive(registry, "lifecycle-p2-n-active");
    const auto jailedAddr = registerActive(registry, "lifecycle-p2-n-jailed");
    registry.jailValidator(jailedAddr, 99, kTs + 1);

    requireCondition(
        registry.isEligibleForConsensus(activeAddr),
        "ACTIVE validator must be eligible"
    );
    requireCondition(
        !registry.isEligibleForConsensus(jailedAddr),
        "JAILED validator must not be eligible"
    );
}

// ── 15. Multiple validators: only ACTIVE appear in eligible list ──────────────

void testMixedStatusEligibleList() {
    ValidatorRegistry registry;
    const auto a1 = registerActive(registry, "lifecycle-p2-o-1");
    const auto a2 = registerActive(registry, "lifecycle-p2-o-2");
    const auto a3 = registerActive(registry, "lifecycle-p2-o-3");

    // Jail a2, request exit for a3
    registry.jailValidator(a2, 10, kTs + 1);
    registry.requestExit(a3, 100, kTs + 1);

    const auto eligible = registry.eligibleValidatorAddresses();
    requireCondition(eligible.size() == 1U, "Only one ACTIVE validator expected");
    requireCondition(eligible[0] == a1, "Only a1 must be eligible");
}

} // namespace

int main() {
    try {
        testRegisterCreatesEntry();
        testPendingAndEligibleAreSeparate();
        testActivateAlreadyActiveReturnsAlreadyActive();
        testJailActiveValidator();
        testJailAlreadyJailedRejected();
        testUnjailBeforeEpochRejected();
        testUnjailAtEpoch();
        testExitRequestFromActive();
        testExitRequestFromJailed();
        testCompleteExit();
        testCompleteExitWithoutRequest();
        testUpdateStake();
        testDeactivateValidator();
        testIsEligibleForConsensus();
        testMixedStatusEligibleList();

        std::cout << "Nodo validator lifecycle Phase 2 tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo validator lifecycle Phase 2 tests FAILED: "
                  << e.what() << "\n";
        return 1;
    }
}
