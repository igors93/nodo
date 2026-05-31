#include "node/ProtectionTreasury.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/LockedStakePosition.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::KeyPair;
using nodo::node::GenesisTreasurySnapshot;
using nodo::node::ProtectionRewardBudget;
using nodo::node::ProtectionRewardGrant;
using nodo::node::ProtectionTreasury;
using nodo::node::RewardDistribution;
using nodo::node::RewardDistributionCalculator;
using nodo::node::SecurityScoreRecord;
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
    return KeyPair::createDevelopmentKeyPair(
        "protection-treasury-validator"
    );
}

GenesisConfig genesisConfig(
    const std::string& validatorAddress
) {
    const BootstrapValidatorConfig validator(
        validatorKeyPair().publicKey(),
        1,
        1,
        "protection-treasury-validator"
    );

    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        1900000000,
        {validator},
        {
            GenesisAccountConfig(
                validatorAddress,
                Amount::fromRawUnits(1000000),
                0
            ),
            GenesisAccountConfig(
                ProtectionTreasury::TREASURY_ADDRESS,
                Amount::fromRawUnits(1000000),
                0
            )
        },
        "protection-treasury-genesis"
    );
}

void testBuildsGenesisTreasurySnapshot() {
    const std::string validatorAddress =
        validatorKeyPair().publicKey().fingerprint();

    const GenesisTreasurySnapshot snapshot =
        ProtectionTreasury::buildGenesisTreasurySnapshot(
            genesisConfig(validatorAddress),
            1
        );

    requireCondition(
        snapshot.active() &&
        snapshot.treasuryAddress() == ProtectionTreasury::TREASURY_ADDRESS &&
        snapshot.genesisTreasuryBalance().rawUnits() == 1000000 &&
        snapshot.protectionBudget().rawUnits() == 100000 &&
        snapshot.protectedReserve().rawUnits() == 900000 &&
        snapshot.availableBalance().rawUnits() == 1000000,
        "Genesis treasury snapshot should reserve ninety percent and expose ten percent as protection budget."
    );
}

void testBuildsProtectionRewardBudget() {
    const std::string validatorAddress =
        validatorKeyPair().publicKey().fingerprint();

    const GenesisTreasurySnapshot snapshot =
        ProtectionTreasury::buildGenesisTreasurySnapshot(
            genesisConfig(validatorAddress),
            1
        );

    const std::vector<RewardDistribution> rewards =
        RewardDistributionCalculator::buildForVoters(
            Amount::fromRawUnits(50000),
            {validatorAddress},
            1
        );

    const ProtectionRewardBudget budget =
        ProtectionTreasury::buildProtectionRewardBudget(
            snapshot,
            rewards
        );

    requireCondition(
        budget.active() &&
        budget.availableBudget().rawUnits() == 100000 &&
        budget.plannedTotal().rawUnits() == 50000 &&
        budget.remainingBudget().rawUnits() == 50000 &&
        budget.beneficiaryCount() == 1,
        "Protection reward budget should plan rewards within treasury budget."
    );
}

void testBuildsScoreWeightedProtectionRewardGrant() {
    const std::string validatorAddress =
        validatorKeyPair().publicKey().fingerprint();

    const GenesisTreasurySnapshot snapshot =
        ProtectionTreasury::buildGenesisTreasurySnapshot(
            genesisConfig(validatorAddress),
            1
        );

    const std::vector<RewardDistribution> rewards =
        RewardDistributionCalculator::buildForVoters(
            Amount::fromRawUnits(50000),
            {validatorAddress},
            1
        );

    const ProtectionRewardBudget budget =
        ProtectionTreasury::buildProtectionRewardBudget(
            snapshot,
            rewards
        );

    const std::vector<SecurityScoreRecord> scores = {
        SecurityScoreRecord(
            validatorAddress,
            1,
            100,
            90,
            10,
            0,
            0,
            "TEST_SCORE",
            "source"
        )
    };

    const std::vector<ProtectionRewardGrant> grants =
        ProtectionTreasury::buildProtectionRewardGrants(
            budget,
            rewards,
            scores
        );

    requireCondition(
        grants.size() == 1U &&
        grants.front().isValid() &&
        grants.front().validatorAddress() == validatorAddress &&
        grants.front().plannedReward().rawUnits() == 50000 &&
        grants.front().securityScore() == 100,
        "Protection reward grant should be deterministic and score-weighted."
    );
}

} // namespace

int main() {
    try {
        testBuildsGenesisTreasurySnapshot();
        testBuildsProtectionRewardBudget();
        testBuildsScoreWeightedProtectionRewardGrant();

        std::cout << "Nodo protection treasury tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protection treasury tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
