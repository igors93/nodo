#include "node/MonetaryFirewall.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::KeyPair;
using nodo::node::MonetaryFirewall;
using nodo::node::MonetaryFirewallAudit;
using nodo::node::MonetaryPolicy;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "monetary-firewall-validator"
    );
}

GenesisConfig genesisConfig() {
    const BootstrapValidatorConfig validator(
        validatorKeyPair().publicKey(),
        1,
        1,
        "monetary-firewall-validator"
    );

    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        1900000000,
        {validator},
        {
            GenesisAccountConfig(
                validator.validatorAddress(),
                Amount::fromRawUnits(1000000),
                0
            ),
            GenesisAccountConfig(
                "treasury-protocol-account",
                Amount::fromRawUnits(4000000),
                0
            )
        },
        "monetary-firewall-genesis"
    );
}

void testGenesisSupply() {
    requireCondition(
        MonetaryFirewall::genesisSupply(genesisConfig()).rawUnits() == 5000000,
        "Genesis supply should equal the sum of genesis account balances."
    );
}

void testAnnualMintLimit() {
    const MonetaryPolicy policy =
        MonetaryPolicy::protocolDefault();

    requireCondition(
        policy.isValid(),
        "Default monetary policy should be valid."
    );

    requireCondition(
        MonetaryFirewall::annualMintLimit(
            Amount::fromRawUnits(1000000),
            policy
        ).rawUnits() == 40000,
        "Annual mint limit should be four percent of base supply."
    );
}

void testZeroMintAuditPasses() {
    const MonetaryFirewallAudit audit =
        MonetaryFirewall::buildZeroMintAudit(
            genesisConfig(),
            1
        );

    requireCondition(
        audit.isValid() &&
        audit.passed() &&
        audit.supplyLedger().minted().rawUnits() == 0 &&
        audit.supplyLedger().burned().rawUnits() == 0 &&
        audit.supplyLedger().supplyBefore().rawUnits() == 5000000 &&
        audit.supplyLedger().supplyAfter().rawUnits() == 5000000,
        "Zero-mint monetary firewall audit should pass without changing supply."
    );
}

void testRejectsInflationAboveCap() {
    bool rejected = false;

    try {
        MonetaryFirewall::buildAudit(
            genesisConfig(),
            1,
            Amount::fromRawUnits(300000),
            Amount::fromRawUnits(0),
            Amount::fromRawUnits(0),
            Amount::fromRawUnits(0)
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Monetary firewall should reject minting above the annual cap."
    );
}

void testCanonicalEpochRewardAuditPassesWithinCap() {
    const MonetaryFirewallAudit audit =
        MonetaryFirewall::buildEpochRewardAuditWithSupplyBefore(
            43201, Amount::fromRawUnits(5000000),
            Amount::fromRawUnits(1000), Amount::fromRawUnits(20),
            Amount::fromRawUnits(10), Amount::fromRawUnits(0)
        );
    requireCondition(
        audit.passed() && audit.supplyLedger().minted().rawUnits() == 1000 &&
        audit.supplyLedger().supplyAfter().rawUnits() == 5000980,
        "Authorized epoch reward audit should account for mint and burn atomically."
    );
}

} // namespace

int main() {
    try {
        testGenesisSupply();
        testAnnualMintLimit();
        testZeroMintAuditPasses();
        testRejectsInflationAboveCap();
        testCanonicalEpochRewardAuditPassesWithinCap();

        std::cout << "Nodo monetary firewall tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo monetary firewall tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
