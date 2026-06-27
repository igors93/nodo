#include "consensus/SlashingEvidence.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::consensus::SlashingEvidenceRecord;
using nodo::consensus::SlashingEvidenceSeverity;
using nodo::consensus::SlashingEvidenceType;
using nodo::consensus::ValidatorPenaltyLedger;
using nodo::consensus::ValidatorPenaltyPolicy;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::crypto::AddressDerivation;
using nodo::crypto::KeyPair;

constexpr std::int64_t kNow = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

// Build a ValidatorRegistry with one active validator.
std::pair<ValidatorRegistry, std::string> registryWithOneValidator() {
    const KeyPair kp = KeyPair::createDeterministicBls12381KeyPair("gap2-test-key");
    const std::string address =
        AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();

    ValidatorRegistrationRecord reg(
        address,
        kp.publicKey(),
        1,
        "gap2-meta",
        kNow
    );

    ValidatorRegistry registry;
    const auto regResult = registry.registerValidator(reg);
    if (!regResult.accepted()) {
        throw std::runtime_error("Registry registration must succeed.");
    }
    registry.activateValidator(address, 1, kNow);

    return {std::move(registry), address};
}

SlashingEvidenceRecord makeDoubleVoteEvidence(
    const std::string& evidenceId,
    const std::string& validatorAddress
) {
    return SlashingEvidenceRecord(
        evidenceId,
        SlashingEvidenceType::DOUBLE_VOTE,
        validatorAddress,
        "payload-hash-" + evidenceId,
        SlashingEvidenceSeverity::SLASHABLE,
        kNow
    );
}

void testApplyEvidenceJailsValidator() {
    auto [registry, address] = registryWithOneValidator();

    require(registry.isActiveValidator(address),
            "Validator must be active before evidence is applied.");

    ValidatorPenaltyLedger ledger;
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeTestnetPolicy();

    const auto evidence = makeDoubleVoteEvidence("evidence-001", address);
    const auto result = ledger.applyEvidenceWithRegistryEffect(
        evidence, policy, kNow, 0, registry
    );

    require(result.applied(),
            "Evidence application must succeed for SLASHABLE evidence.");
    require(result.decision().has_value(),
            "Applied result must carry a penalty decision.");

    // After the registry effect is applied, the validator must be jailed
    // or deactivated (depending on the policy decision).
    const bool jailed = ledger.containsEvidence(evidence.evidenceId());
    require(jailed, "Ledger must record the evidence after application.");

    const auto* entry = registry.entryForAddress(address);
    require(entry != nullptr, "Registry entry must exist for the validator.");
    // Conservative testnet policy either jails or tombstones:
    const bool penaltyApplied =
        entry->jailed() ||
        entry->status() != nodo::core::ValidatorRegistrationStatus::ACTIVE;
    require(penaltyApplied,
            "Registry must reflect jail or deactivation after evidence application.");
}

void testDoubleEvidenceIsIdempotent() {
    auto [registry, address] = registryWithOneValidator();

    ValidatorPenaltyLedger ledger;
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeTestnetPolicy();
    const auto evidence = makeDoubleVoteEvidence("evidence-002", address);

    const auto first = ledger.applyEvidenceWithRegistryEffect(
        evidence, policy, kNow, 0, registry
    );
    require(first.applied(), "First application must succeed.");

    // Second application for the same evidence ID must be idempotent.
    const auto second = ledger.applyEvidenceWithRegistryEffect(
        evidence, policy, kNow, 0, registry
    );
    require(!second.applied(),
            "Second application of the same evidence must not re-apply.");
}

} // namespace

int main() {
    try {
        testApplyEvidenceJailsValidator();
        testDoubleEvidenceIsIdempotent();

        std::cout << "Nodo Gap2 penalty-to-registry tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo Gap2 penalty-to-registry tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
