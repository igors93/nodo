#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/CoinLot.hpp"
#include "core/CoinLotRegistry.hpp"
#include "core/CoinLotRegistryRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/GenesisRewardRecord.hpp"
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
using nodo::core::CoinLot;
using nodo::core::CoinLotRegistry;
using nodo::core::CoinLotRegistryRebuilder;
using nodo::core::CoinLotStatus;
using nodo::core::LedgerRecord;
using nodo::economics::GenesisRewardReason;
using nodo::economics::GenesisRewardRecord;
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

CoinLot makeLot(
    const std::string& id,
    const std::string& origin,
    const std::string& owner,
    std::int64_t wholeNodo,
    CoinLotStatus status = CoinLotStatus::AVAILABLE
) {
    return CoinLot(
        id,
        origin,
        owner,
        Amount::fromNodo(wholeNodo),
        status,
        1,
        0,
        kTimestamp
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

Blockchain sampleBlockchainWithRewards() {
    const ValidationWorkRecord work(
        "nodo1validator",
        1,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::ACCEPTED,
        "target-hash",
        "evidence-hash",
        10,
        kTimestamp
    );

    const ValidatorScoreRecord correctedScore(
        "nodo1validator",
        1,
        50,
        55,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence",
        kTimestamp + 1
    );

    const ProtectionEpoch epoch(
        1,
        0,
        10,
        Amount::fromNodo(5),
        Amount::fromNodo(100),
        7500
    );

    const GenesisRewardRecord rewardA =
        genesisReward(
            "nodo1validator",
            30
        );

    const GenesisRewardRecord rewardB(
        1,
        "nodo1validator2",
        Amount::fromNodo(20),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-hash-validator2",
        "NODO_EPOCH_EMISSION_POLICY_V1",
        "accepted-block-hash-validator2",
        kTimestamp + 2
    );

    const std::vector<LedgerRecord> records = {
        LedgerRecord::fromValidationWorkRecord(work, kTimestamp),
        LedgerRecord::fromValidatorScoreRecord(correctedScore, kTimestamp + 1),
        LedgerRecord::fromProtectionEpoch(epoch, kTimestamp + 2),
        LedgerRecord::fromGenesisRewardRecord(rewardA, kTimestamp + 3),
        LedgerRecord::fromGenesisRewardRecord(rewardB, kTimestamp + 4)
    };

    const Block genesis =
        Block::createGenesisBlock(
            records,
            kTimestamp + 5
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);

    return blockchain;
}

void testRegistryAddsAndVerifiesLots() {
    CoinLotRegistry registry;

    const CoinLot lot =
        makeLot(
            "lot-1",
            "origin-1",
            "nodo1owner",
            10
        );

    registry.addLot(lot);

    requireCondition(
        registry.size() == 1U,
        "Registry should contain one lot."
    );

    requireCondition(
        registry.hasLot("lot-1"),
        "Registry should find inserted lot."
    );

    requireCondition(
        registry.verifyExists("lot-1").success(),
        "Existing lot should verify."
    );

    requireCondition(
        registry.verifySpendable("lot-1", "nodo1owner").success(),
        "Owner should be able to verify spendable lot."
    );

    requireCondition(
        !registry.verifySpendable("lot-1", "nodo1wrong").success(),
        "Wrong owner should not verify spendable lot."
    );

    requireCondition(
        registry.verifyExactSpend(
            "lot-1",
            "nodo1owner",
            Amount::fromNodo(10)
        ).success(),
        "Exact amount should verify."
    );

    requireCondition(
        !registry.verifyExactSpend(
            "lot-1",
            "nodo1owner",
            Amount::fromNodo(9)
        ).success(),
        "Wrong amount should not verify."
    );
}

void testDuplicateLotsAreRejected() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "duplicate-lot",
            "origin",
            "nodo1owner",
            5
        )
    );

    bool rejected = false;

    try {
        registry.addLot(
            makeLot(
                "duplicate-lot",
                "origin",
                "nodo1owner",
                5
            )
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Registry accepted duplicate lot id."
    );
}

void testConsumeLotAndCreateOutputs() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "input-lot",
            "origin-input",
            "nodo1igor",
            50
        )
    );

    const std::vector<CoinLot> outputs = {
        makeLot(
            "recipient-lot",
            "input-lot",
            "nodo1ana",
            20
        ),
        makeLot(
            "change-lot",
            "input-lot",
            "nodo1igor",
            29
        ),
        makeLot(
            "fee-lot",
            "input-lot",
            "nodo1fee",
            1
        )
    };

    registry.consumeLotAndCreateOutputs(
        "input-lot",
        "nodo1igor",
        outputs
    );

    requireCondition(
        registry.lot("input-lot").isSpent(),
        "Input lot should be marked spent."
    );

    requireCondition(
        registry.lot("recipient-lot").isAvailable(),
        "Recipient output lot should be available."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1ana") == Amount::fromNodo(20),
        "Recipient available balance is wrong."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1igor") == Amount::fromNodo(29),
        "Change balance is wrong."
    );

    requireCondition(
        registry.totalAvailableAmount() == Amount::fromNodo(50),
        "Total available output amount is wrong."
    );

    requireCondition(
        registry.totalSpentAmount() == Amount::fromNodo(50),
        "Total spent amount is wrong."
    );

    bool doubleSpendRejected = false;

    try {
        registry.markSpent(
            "input-lot",
            "nodo1igor"
        );
    } catch (const std::exception&) {
        doubleSpendRejected = true;
    }

    requireCondition(
        doubleSpendRejected,
        "Registry allowed double spend of spent input lot."
    );
}

void testInvalidSplitIsRejected() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "input-lot",
            "origin-input",
            "nodo1igor",
            50
        )
    );

    const std::vector<CoinLot> badOutputs = {
        makeLot(
            "recipient-lot",
            "input-lot",
            "nodo1ana",
            20
        ),
        makeLot(
            "change-lot",
            "input-lot",
            "nodo1igor",
            20
        )
    };

    bool rejected = false;

    try {
        registry.consumeLotAndCreateOutputs(
            "input-lot",
            "nodo1igor",
            badOutputs
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Registry accepted output sum that did not match input amount."
    );

    requireCondition(
        registry.lot("input-lot").isAvailable(),
        "Input lot should remain available after failed split."
    );
}

void testLockedAndSlashedLotsAreNotSpendable() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "lock-lot",
            "origin-lock",
            "nodo1owner",
            10
        )
    );

    registry.lockForSecurity(
        "lock-lot",
        "nodo1owner",
        100
    );

    requireCondition(
        registry.lot("lock-lot").isLockedForSecurity(),
        "Lot should be locked for security."
    );

    requireCondition(
        !registry.verifySpendable("lock-lot", "nodo1owner").success(),
        "Locked lot should not be spendable."
    );

    requireCondition(
        registry.lockedBalanceForOwner("nodo1owner") == Amount::fromNodo(10),
        "Locked balance is wrong."
    );

    registry.unlockIfMature(
        "lock-lot",
        100
    );

    requireCondition(
        registry.lot("lock-lot").isAvailable(),
        "Mature locked lot should unlock."
    );

    registry.markSlashed("lock-lot");

    requireCondition(
        registry.lot("lock-lot").isSlashed(),
        "Lot should be slashed."
    );

    requireCondition(
        !registry.verifySpendable("lock-lot", "nodo1owner").success(),
        "Slashed lot should not be spendable."
    );
}

void testRegistryRebuildsRewardLotsFromBlockchain() {
    const Blockchain blockchain =
        sampleBlockchainWithRewards();

    requireCondition(
        blockchain.isValid(),
        "Sample reward blockchain should be valid."
    );

    const CoinLotRegistry registry =
        CoinLotRegistryRebuilder::rebuildRewardLotsFromBlockchain(
            blockchain
        );

    requireCondition(
        registry.isValid(),
        "Rebuilt reward CoinLot registry should be valid."
    );

    requireCondition(
        registry.size() == 2U,
        "Rebuilt reward CoinLot registry should have two lots."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1validator") == Amount::fromNodo(30),
        "Validator reward balance is wrong."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1validator2") == Amount::fromNodo(20),
        "Second validator reward balance is wrong."
    );

    requireCondition(
        registry.totalAvailableAmount() == Amount::fromNodo(50),
        "Total reward lots available amount is wrong."
    );
}

} // namespace

int main() {
    try {
        testRegistryAddsAndVerifiesLots();
        testDuplicateLotsAreRejected();
        testConsumeLotAndCreateOutputs();
        testInvalidSplitIsRejected();
        testLockedAndSlashedLotsAreNotSpendable();
        testRegistryRebuildsRewardLotsFromBlockchain();

        std::cout << "Nodo coin lot registry tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo coin lot registry tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
