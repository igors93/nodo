#include "node/ControlledIssuance.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtectionTreasury.hpp"
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
using nodo::node::ControlledIssuance;
using nodo::node::InflationEpochSnapshot;
using nodo::node::MintAuthorizationRecord;
using nodo::node::MonetaryFirewall;
using nodo::node::ProtectionTreasury;
using nodo::node::SupplyExpansionRecord;
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
        "controlled-issuance-validator"
    );
}

GenesisConfig genesisConfig() {
    const KeyPair keyPair =
        validatorKeyPair();

    const BootstrapValidatorConfig validator(
        keyPair.publicKey(),
        1,
        1,
        "controlled-issuance-validator"
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
                ProtectionTreasury::TREASURY_ADDRESS,
                Amount::fromRawUnits(4000000),
                0
            )
        },
        "controlled-issuance-genesis"
    );
}

void testBuildsInflationEpochSnapshot() {
    const InflationEpochSnapshot snapshot =
        ControlledIssuance::buildInflationEpochSnapshot(
            genesisConfig(),
            1,
            Amount::fromRawUnits(0)
        );

    requireCondition(
        snapshot.active() &&
        snapshot.blockHeight() == 1U &&
        snapshot.epochStartBlock() == 1U &&
        snapshot.epochEndBlock() == nodo::node::NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS &&
        snapshot.maxAnnualInflationBasisPoints() == nodo::node::NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS &&
        snapshot.baseSupply().rawUnits() == 5000000 &&
        snapshot.annualMintLimit().rawUnits() == 200000 &&
        snapshot.mintedThisEpoch().rawUnits() == 0 &&
        snapshot.remainingMintCapacity().rawUnits() == 200000,
        "Inflation epoch snapshot should expose the annual cap and remaining mint capacity."
    );
}

void testBuildsNoMintAuthorization() {
    const InflationEpochSnapshot snapshot =
        ControlledIssuance::buildInflationEpochSnapshot(
            genesisConfig(),
            1,
            Amount::fromRawUnits(0)
        );

    const MintAuthorizationRecord authorization =
        ControlledIssuance::buildNoMintAuthorization(
            snapshot
        );

    requireCondition(
        authorization.isValid() &&
        authorization.status() == "NONE" &&
        authorization.authorizedAmount().rawUnits() == 0 &&
        authorization.requiredApprovalBasisPoints() == ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS &&
        authorization.timelockBlocks() == nodo::node::NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS &&
        authorization.governanceDigest() == ControlledIssuance::GOVERNANCE_LOCKED_DIGEST,
        "Default mint authorization should make issuance impossible until a future governance phase."
    );
}

void testBuildsNoSupplyExpansion() {
    const InflationEpochSnapshot snapshot =
        ControlledIssuance::buildInflationEpochSnapshot(
            genesisConfig(),
            1,
            Amount::fromRawUnits(0)
        );

    const MintAuthorizationRecord authorization =
        ControlledIssuance::buildNoMintAuthorization(
            snapshot
        );

    const SupplyExpansionRecord expansion =
        ControlledIssuance::buildNoSupplyExpansion(
            authorization,
            snapshot
        );

    requireCondition(
        expansion.isValid() &&
        expansion.status() == "NONE" &&
        expansion.mintedAmount().rawUnits() == 0 &&
        expansion.recipientAddress() == ControlledIssuance::NO_RECIPIENT_ADDRESS &&
        expansion.authorizationId() == authorization.authorizationId(),
        "Default supply expansion should record that no mint was executed."
    );
}

void testRejectsEpochAboveAnnualCap() {
    bool rejected = false;

    try {
        ControlledIssuance::buildInflationEpochSnapshot(
            genesisConfig(),
            1,
            Amount::fromRawUnits(200001)
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Controlled issuance should reject minted amounts above the annual cap."
    );
}

} // namespace

int main() {
    try {
        testBuildsInflationEpochSnapshot();
        testBuildsNoMintAuthorization();
        testBuildsNoSupplyExpansion();
        testRejectsEpochAboveAnnualCap();

        std::cout << "Nodo controlled issuance tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo controlled issuance tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
