#include "core/GenesisVerifier.hpp"

#include "crypto/KeyPair.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

nodo::config::BootstrapValidatorConfig makeValidator(const std::string& seed) {
    return nodo::config::BootstrapValidatorConfig(
        nodo::crypto::KeyPair::createDeterministicBls12381KeyPair(seed).publicKey(),
        1,
        100,
        "meta-hash"
    );
}

nodo::config::GenesisConfig validGenesis() {
    return nodo::config::GenesisConfig(
        nodo::config::NetworkParameters::developmentLocal(),
        1900000000,
        {makeValidator("genesis-validator-test-a"), makeValidator("genesis-validator-test-b")},
        "test-genesis"
    );
}

void testValidGenesisPassesVerification() {
    const auto result = nodo::core::GenesisVerifier::verify(validGenesis());
    assert(result.isValid());
    assert(result.status() == nodo::core::GenesisVerificationStatus::VALID);
}

void testEmptyValidatorSetRejected() {
    const nodo::config::GenesisConfig genesis(
        nodo::config::NetworkParameters::developmentLocal(),
        1900000000,
        {},
        "empty-validators"
    );
    const auto result = nodo::core::GenesisVerifier::verify(genesis);
    assert(!result.isValid());
    assert(result.status() == nodo::core::GenesisVerificationStatus::EMPTY_VALIDATOR_SET);
}

void testDuplicateValidatorRejected() {
    // Same seed → same key → same address → duplicate
    const nodo::config::GenesisConfig genesis(
        nodo::config::NetworkParameters::developmentLocal(),
        1900000000,
        {makeValidator("same-seed-dup"), makeValidator("same-seed-dup")},
        "dup-validators"
    );
    const auto result = nodo::core::GenesisVerifier::verify(genesis);
    assert(!result.isValid());
    assert(result.status() == nodo::core::GenesisVerificationStatus::VALIDATOR_DUPLICATE);
}

void testInvalidTimestampRejected() {
    const nodo::config::GenesisConfig genesis(
        nodo::config::NetworkParameters::developmentLocal(),
        0,
        {makeValidator("genesis-validator-ts-test")},
        "bad-timestamp"
    );
    const auto result = nodo::core::GenesisVerifier::verify(genesis);
    assert(!result.isValid());
    assert(result.status() == nodo::core::GenesisVerificationStatus::INVALID_TIMESTAMP);
}

void testGenesisWithAccountsPassesVerification() {
    const nodo::config::GenesisConfig genesis(
        nodo::config::NetworkParameters::developmentLocal(),
        1900000000,
        {makeValidator("genesis-validator-acct-test")},
        {nodo::config::GenesisAccountConfig(
            "nodo1testaddress001",
            nodo::utils::Amount::fromRawUnits(1000),
            0
        )},
        "genesis-with-accounts"
    );
    const auto result = nodo::core::GenesisVerifier::verify(genesis);
    assert(result.isValid());
}

void testSupplyBalanceMismatchRejected() {
    const auto result = nodo::core::GenesisVerifier::verifySupplyBalance(
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(200)
    );
    assert(!result.isValid());
    assert(result.status() == nodo::core::GenesisVerificationStatus::SUPPLY_MISMATCH);
}

void testSupplyBalanceMatchPasses() {
    const auto result = nodo::core::GenesisVerifier::verifySupplyBalance(
        nodo::utils::Amount::fromRawUnits(500),
        nodo::utils::Amount::fromRawUnits(500)
    );
    assert(result.isValid());
}

void testNegativeSupplyRejected() {
    // Amount constructor now rejects negative values; the type-level guard
    // replaces the old NEGATIVE_SUPPLY status path in GenesisVerifier.
    bool threw = false;
    try {
        (void)nodo::utils::Amount(-1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testStatusToString() {
    assert(nodo::core::genesisVerificationStatusToString(
               nodo::core::GenesisVerificationStatus::VALID) == "VALID");
    assert(nodo::core::genesisVerificationStatusToString(
               nodo::core::GenesisVerificationStatus::VALIDATOR_DUPLICATE) == "VALIDATOR_DUPLICATE");
}

} // namespace

int main() {
    testValidGenesisPassesVerification();
    testEmptyValidatorSetRejected();
    testDuplicateValidatorRejected();
    testInvalidTimestampRejected();
    testGenesisWithAccountsPassesVerification();
    testSupplyBalanceMismatchRejected();
    testSupplyBalanceMatchPasses();
    testNegativeSupplyRejected();
    testStatusToString();
    return 0;
}
