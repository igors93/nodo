#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::ChainStateRebuilder;
using nodo::core::LedgerRecord;
using nodo::core::State;
using nodo::core::StateRebuildReport;
using nodo::core::Transaction;
using nodo::core::TransactionType;
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

GenesisRewardRecord reward(
    const std::string& validator,
    std::int64_t wholeNodo,
    std::int64_t timestampOffset
) {
    return GenesisRewardRecord(
        1,
        validator,
        Amount::fromNodo(wholeNodo),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-" + validator,
        "NODO_EPOCH_EMISSION_POLICY_V1",
        "accepted-block-hash-" + validator,
        kTimestamp + timestampOffset
    );
}

Blockchain buildRewardBlockchain() {
    const GenesisRewardRecord rewardA =
        reward("nodo1validatorA", 100, 0);

    const GenesisRewardRecord rewardB =
        reward("nodo1validatorB", 50, 1);

    const std::vector<LedgerRecord> genesisRecords = {
        LedgerRecord::fromValidationWorkRecord(
            ValidationWorkRecord(
                "nodo1validatorA",
                1,
                ValidationWorkType::VERIFY_COIN_EXISTENCE,
                ValidationWorkResult::ACCEPTED,
                "target-a",
                "evidence-a",
                10,
                kTimestamp
            ),
            kTimestamp
        ),
        LedgerRecord::fromValidatorScoreRecord(
            ValidatorScoreRecord(
                "nodo1validatorA",
                1,
                50,
                55,
                ValidatorScoreReason::CONSISTENT_VALIDATION,
                "score-evidence-a",
                kTimestamp
            ),
            kTimestamp + 1
        ),
        LedgerRecord::fromProtectionEpoch(
            ProtectionEpoch(
                1,
                0,
                10,
                Amount::fromNodo(5),
                Amount::fromNodo(100),
                5000
            ),
            kTimestamp + 2
        ),
        LedgerRecord::fromGenesisRewardRecord(
            rewardA,
            kTimestamp + 3
        )
    };

    const Block genesis =
        Block::createGenesisBlock(
            genesisRecords,
            kTimestamp + 4
        );

    Transaction transfer(
        TransactionType::TRANSFER,
        "nodo1validatorA",
        "nodo1alice",
        Amount::fromNodo(20),
        Amount::fromNodo(1),
        1,
        kTimestamp + 5,
        {rewardA.createRewardCoinLot(0).id()}
    );

    const nodo::crypto::PublicKey publicKey(
        nodo::crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "genesis-reward-state-flow-test-public-key"
    );

    const nodo::crypto::PrivateKey privateKey(
        nodo::crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "genesis-reward-state-flow-test-private-key"
    );

    transfer.attachSignatureBundle(
        nodo::crypto::SignatureBundle::createDevelopmentSignature(
            transfer.signingPayload(),
            publicKey,
            privateKey,
            kTimestamp + 6
        )
    );

    const std::vector<LedgerRecord> secondRecords = {
        LedgerRecord::fromGenesisRewardRecord(
            rewardB,
            kTimestamp + 6
        ),
        LedgerRecord::fromTransaction(
            transfer,
            nodo::crypto::CryptoPolicy::developmentPolicy(),
            nodo::crypto::SecurityContext::DEVELOPMENT_ONLY,
            kTimestamp + 7
        )
    };

    const Block second(
        1,
        genesis.hash(),
        secondRecords,
        kTimestamp + 8
    );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);
    blockchain.addBlock(second);

    return blockchain;
}

void testStateAppliesGenesisRewardDirectly() {
    State state;

    const GenesisRewardRecord genesisReward =
        reward("nodo1validator", 42, 0);

    state.applyGenesisRewardRecord(genesisReward);

    requireCondition(
        state.totalSupply() == Amount::fromNodo(42),
        "State total supply should include GenesisReward amount."
    );

    requireCondition(
        state.balanceOf("nodo1validator") == Amount::fromNodo(42),
        "Validator balance should include GenesisReward CoinLot."
    );

    requireCondition(
        state.genesisRewardRecords().size() == 1U,
        "State should store accepted GenesisRewardRecord."
    );

    requireCondition(
        state.mintRecords().empty(),
        "GenesisReward path should not create legacy MintRecord."
    );

    requireCondition(
        state.isSupplyAuditable(),
        "State supply should be auditable after GenesisReward."
    );

    bool duplicateRejected = false;

    try {
        state.applyGenesisRewardRecord(genesisReward);
    } catch (const std::exception&) {
        duplicateRejected = true;
    }

    requireCondition(
        duplicateRejected,
        "Duplicate GenesisRewardRecord should be rejected."
    );
}

void testGenesisRewardDeserializesRoundTrip() {
    const GenesisRewardRecord original =
        reward("nodo1validator", 12, 0);

    const GenesisRewardRecord loaded =
        GenesisRewardRecord::deserialize(
            original.serialize()
        );

    requireCondition(
        loaded.serialize() == original.serialize(),
        "GenesisRewardRecord round-trip serialization mismatch."
    );

    requireCondition(
        loaded.deterministicId() == original.deterministicId(),
        "GenesisRewardRecord deterministic id changed after deserialize."
    );
}

void testChainStateRebuilderAppliesGenesisRewards() {
    const Blockchain blockchain =
        buildRewardBlockchain();

    requireCondition(
        blockchain.isValid(),
        "Reward blockchain should be valid."
    );

    const StateRebuildReport report =
        ChainStateRebuilder::auditBlockchain(blockchain);

    requireCondition(
        report.success(),
        "Reward blockchain audit should succeed."
    );

    requireCondition(
        report.genesisRewardRecordCount() == 2U,
        "Audit should count GenesisReward records."
    );

    requireCondition(
        report.protectionMetadataRecordCount() == 3U,
        "Audit should count protection metadata records."
    );

    const State state =
        ChainStateRebuilder::rebuildStateFromLedgerRecords(blockchain);

    requireCondition(
        state.isSupplyAuditable(),
        "Rebuilt reward state should be supply auditable."
    );

    requireCondition(
        state.totalSupply() == Amount::fromNodo(150),
        "Rebuilt state total supply should come from GenesisReward records."
    );

    requireCondition(
        state.balanceOf("nodo1validatorA") == Amount::fromNodo(79),
        "Validator A balance should reflect explicit-input transfer and fee."
    );

    requireCondition(
        state.balanceOf("nodo1validatorB") == Amount::fromNodo(50),
        "Validator B balance should reflect GenesisReward."
    );

    requireCondition(
        state.balanceOf("nodo1alice") == Amount::fromNodo(20),
        "Recipient balance should reflect transfer from GenesisReward lot."
    );

    requireCondition(
        state.balanceOf(State::feePoolAddress()) == Amount::fromNodo(1),
        "Fee pool balance should reflect transfer fee."
    );

    requireCondition(
        state.mintRecords().empty(),
        "Reward-only chain should not create legacy MintRecords."
    );

    requireCondition(
        state.genesisRewardRecords().size() == 2U,
        "Rebuilt state should store GenesisReward records."
    );
}

void testGenesisRewardOnlyRebuild() {
    const Blockchain blockchain =
        buildRewardBlockchain();

    const State state =
        ChainStateRebuilder::rebuildStateFromGenesisRewardRecords(blockchain);

    requireCondition(
        state.totalSupply() == Amount::fromNodo(150),
        "GenesisReward-only rebuild should include reward supply."
    );

    requireCondition(
        state.balanceOf("nodo1validatorA") == Amount::fromNodo(100),
        "GenesisReward-only rebuild should ignore transfer records."
    );

    requireCondition(
        state.balanceOf("nodo1validatorB") == Amount::fromNodo(50),
        "GenesisReward-only rebuild should include all rewards."
    );

    requireCondition(
        state.isSupplyAuditable(),
        "GenesisReward-only rebuild should be auditable."
    );
}

} // namespace

int main() {
    try {
        testStateAppliesGenesisRewardDirectly();
        testGenesisRewardDeserializesRoundTrip();
        testChainStateRebuilderAppliesGenesisRewards();
        testGenesisRewardOnlyRebuild();

        std::cout << "Nodo genesis reward state flow tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo genesis reward state flow tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
