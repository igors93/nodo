#include "consensus/ProposerSchedule.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::consensus::ProposerSchedule;
using nodo::crypto::KeyPair;

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
        1900000000
    );
}

// ── 1. Empty registry selects no proposer ────────────────────────────────────

void testEmptyRegistrySelectsNoProposer() {
    ValidatorRegistry registry;

    const std::string proposer = ProposerSchedule::selectProposer(
        registry, "test-chain", 1, 1
    );

    requireCondition(
        proposer.empty(),
        "Empty registry must select no proposer"
    );
}

// ── 2. ACTIVE validator is selected as proposer ──────────────────────────────

void testActiveValidatorIsProposed() {
    ValidatorRegistry registry;
    const auto rec = makeRecord("proposer-elg-b", 1);
    registry.registerValidator(rec);
    registry.activateValidator(rec.validatorAddress(), 1, 1900000001);

    const std::string proposer = ProposerSchedule::selectProposer(
        registry, "test-chain", 1, 1
    );

    requireCondition(
        proposer == rec.validatorAddress(),
        "Single ACTIVE validator must be selected as proposer"
    );
}

// ── 3. JAILED validator excluded from proposer selection ─────────────────────

void testJailedValidatorExcludedFromProposer() {
    ValidatorRegistry registry;
    const auto activeRec = makeRecord("proposer-elg-c-active", 1);
    const auto jailedRec = makeRecord("proposer-elg-c-jailed", 1);

    registry.registerValidator(activeRec);
    registry.registerValidator(jailedRec);
    registry.activateValidator(activeRec.validatorAddress(), 1, 1900000001);
    registry.activateValidator(jailedRec.validatorAddress(), 1, 1900000001);
    registry.jailValidator(jailedRec.validatorAddress(), 99, 1900000002);

    // Over many rounds, the jailed address must never be selected
    bool jailedWasSelected = false;
    for (std::uint64_t round = 1; round <= 20; ++round) {
        const std::string proposer = ProposerSchedule::selectProposer(
            registry, "test-chain-jailed", 1, round
        );
        if (proposer == jailedRec.validatorAddress()) {
            jailedWasSelected = true;
            break;
        }
    }

    requireCondition(
        !jailedWasSelected,
        "Jailed validator must never be selected as proposer"
    );
}

// ── 4. EXIT_REQUESTED validator excluded from proposer selection ──────────────

void testExitRequestedValidatorExcludedFromProposer() {
    ValidatorRegistry registry;
    const auto activeRec = makeRecord("proposer-elg-d-active", 1);
    const auto exitRec   = makeRecord("proposer-elg-d-exit", 1);

    registry.registerValidator(activeRec);
    registry.registerValidator(exitRec);
    registry.activateValidator(activeRec.validatorAddress(), 1, 1900000001);
    registry.activateValidator(exitRec.validatorAddress(), 1, 1900000001);
    registry.requestExit(exitRec.validatorAddress(), 100, 1900000002);

    bool exitWasSelected = false;
    for (std::uint64_t round = 1; round <= 20; ++round) {
        const std::string proposer = ProposerSchedule::selectProposer(
            registry, "test-chain-exit", 1, round
        );
        if (proposer == exitRec.validatorAddress()) {
            exitWasSelected = true;
            break;
        }
    }

    requireCondition(
        !exitWasSelected,
        "EXIT_REQUESTED validator must never be selected as proposer"
    );
}

} // namespace

int main() {
    try {
        testEmptyRegistrySelectsNoProposer();
        testActiveValidatorIsProposed();
        testJailedValidatorExcludedFromProposer();
        testExitRequestedValidatorExcludedFromProposer();

        std::cout << "Nodo ProposerSchedule eligibility tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo ProposerSchedule eligibility tests FAILED: "
                  << e.what() << "\n";
        return 1;
    }
}
