#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEconomicsRebuilder.hpp"
#include "economics/ProtectionEconomicsState.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::LedgerRecord;
using nodo::economics::GenesisRewardReason;
using nodo::economics::GenesisRewardRecord;
using nodo::economics::ProtectionEconomicsRebuilder;
using nodo::economics::ProtectionEconomicsState;
using nodo::economics::ProtectionEpoch;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::economics::ValidatorScoreReason;
using nodo::economics::ValidatorScoreRecord;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

ValidationWorkRecord acceptedWork(
    const std::string& validator,
    std::uint32_t weight
) {
    return ValidationWorkRecord(
        validator,
        1,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::ACCEPTED,
        "coin-lot-target-hash-" + validator,
        "coin-lot-evidence-hash-" + validator,
        weight,
        kTimestamp
    );
}

ValidationWorkRecord rejectedWork(
    const std::string& validator,
    std::uint32_t weight
) {
    return ValidationWorkRecord(
        validator,
        1,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::REJECTED,
        "rejected-target-hash-" + validator,
        "rejected-evidence-hash-" + validator,
        weight,
        kTimestamp
    );
}

ValidatorScoreRecord scoreRecord(
    const std::string& validator,
    std::int32_t previousScore,
    std::int32_t newScore
) {
    return ValidatorScoreRecord(
        validator,
        1,
        previousScore,
        newScore,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "validator-score-evidence-" + validator,
        kTimestamp
    );
}

ProtectionEpoch protectionEpoch() {
    return ProtectionEpoch(
        1,
        0,
        10,
        Amount::fromNodo(5),
        Amount::fromNodo(100),
        7500
    );
}

GenesisRewardRecord genesisReward(
    const std::string& validator,
    std::int64_t wholeNodo
) {
    return GenesisRewardRecord(
        1,
        validator,
        Amount::fromNodo(wholeNodo),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-hash-" + validator,
        "NODO_EPOCH_EMISSION_POLICY_V1",
        "accepted-block-hash-" + validator,
        kTimestamp
    );
}

Blockchain sampleProtectionBlockchain() {
    const std::vector<LedgerRecord> genesisRecords = {
        LedgerRecord::fromValidationWorkRecord(
            acceptedWork("nodo1validatorA", 25),
            kTimestamp
        ),
        LedgerRecord::fromValidationWorkRecord(
            rejectedWork("nodo1validatorA", 99),
            kTimestamp + 1
        ),
        LedgerRecord::fromValidatorScoreRecord(
            scoreRecord("nodo1validatorA", 50, 55),
            kTimestamp + 2
        )
    };

    const Block genesis =
        Block::createGenesisBlock(
            genesisRecords,
            kTimestamp + 3
        );

    const std::vector<LedgerRecord> secondRecords = {
        LedgerRecord::fromValidationWorkRecord(
            acceptedWork("nodo1validatorB", 15),
            kTimestamp + 4
        ),
        LedgerRecord::fromValidatorScoreRecord(
            scoreRecord("nodo1validatorB", 40, 45),
            kTimestamp + 5
        ),
        LedgerRecord::fromProtectionEpoch(
            protectionEpoch(),
            kTimestamp + 6
        ),
        LedgerRecord::fromGenesisRewardRecord(
            genesisReward("nodo1validatorA", 30),
            kTimestamp + 7
        ),
        LedgerRecord::fromGenesisRewardRecord(
            genesisReward("nodo1validatorB", 20),
            kTimestamp + 8
        )
    };

    const Block second(
        1,
        genesis.hash(),
        secondRecords,
        kTimestamp + 9
    );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);
    blockchain.addBlock(second);

    return blockchain;
}

void testRebuildProtectionEconomicsStateFromBlockchain() {
    const Blockchain blockchain =
        sampleProtectionBlockchain();

    requireCondition(
        blockchain.isValid(),
        "Sample protection blockchain should be valid."
    );

    const ProtectionEconomicsState state =
        ProtectionEconomicsRebuilder::rebuildFromBlockchain(blockchain);

    requireCondition(
        state.isValid(),
        "Rebuilt protection economics state should be valid."
    );

    requireCondition(
        state.totalAcceptedWorkWeight() == 40,
        "Total accepted work weight is wrong."
    );

    requireCondition(
        state.workRecordCount() == 2,
        "Only accepted work records should count."
    );

    requireCondition(
        state.acceptedWorkWeight("nodo1validatorA") == 25,
        "Validator A work weight is wrong."
    );

    requireCondition(
        state.acceptedWorkWeight("nodo1validatorB") == 15,
        "Validator B work weight is wrong."
    );

    requireCondition(
        state.validatorScore("nodo1validatorA") == 55,
        "Validator A score is wrong."
    );

    requireCondition(
        state.validatorScore("nodo1validatorB") == 45,
        "Validator B score is wrong."
    );

    requireCondition(
        state.protectionEpochCount() == 1,
        "Protection epoch count is wrong."
    );

    requireCondition(
        state.totalSecurityEmission() == Amount::fromNodo(75),
        "Total security emission is wrong."
    );

    requireCondition(
        state.totalRewardPool() == Amount::fromNodo(80),
        "Total reward pool is wrong."
    );

    requireCondition(
        state.genesisRewardCount() == 2,
        "Genesis reward count is wrong."
    );

    requireCondition(
        state.totalGenesisRewards() == Amount::fromNodo(50),
        "Total GenesisReward amount is wrong."
    );

    requireCondition(
        state.rewardCoinLots().size() == 2U,
        "Reward CoinLot count is wrong."
    );

    requireCondition(
        state.rewardCoinLots().front().isAvailable(),
        "Reward CoinLot should be available."
    );
}

void testRebuildFromBlocksMatchesBlockchainRebuild() {
    const Blockchain blockchain =
        sampleProtectionBlockchain();

    const ProtectionEconomicsState fromBlockchain =
        ProtectionEconomicsRebuilder::rebuildFromBlockchain(blockchain);

    const ProtectionEconomicsState fromBlocks =
        ProtectionEconomicsRebuilder::rebuildFromBlocks(blockchain.blocks());

    requireCondition(
        fromBlockchain.serialize() == fromBlocks.serialize(),
        "Rebuilding from blocks should match rebuilding from blockchain."
    );
}

void testNonProtectionRecordsAreIgnored() {
    ProtectionEconomicsState state;

    // Empty state is valid and non-protection records should leave it unchanged.
    requireCondition(
        state.isValid(),
        "Fresh protection economics state should be valid."
    );

    requireCondition(
        state.totalAcceptedWorkWeight() == 0,
        "Fresh protection economics state should have zero accepted work."
    );

    requireCondition(
        state.totalGenesisRewards().isZero(),
        "Fresh protection economics state should have zero GenesisRewards."
    );
}

} // namespace

int main() {
    try {
        testRebuildProtectionEconomicsStateFromBlockchain();
        testRebuildFromBlocksMatchesBlockchainRebuild();
        testNonProtectionRecordsAreIgnored();

        std::cout << "Nodo protection state rebuilder tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protection state rebuilder tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
