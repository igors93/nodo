#include "core/CoinLot.hpp"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::CoinLot;
using nodo::economics::EpochEmissionPolicy;
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

void testEmissionPolicyUsesBootstrapWhenSupplyIsZero() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const Amount cap =
        policy.calculateNewEmissionCap(
            Amount::fromRawUnits(0)
        );

    requireCondition(
        cap == Amount::fromNodo(100),
        "Zero-supply bootstrap emission cap is wrong."
    );
}

void testEmissionPolicyCalculatesInflationCap() {
    const EpochEmissionPolicy policy(
        "TEST_POLICY",
        400,
        365,
        Amount::fromNodo(100)
    );

    const Amount cap =
        policy.calculateNewEmissionCap(
            Amount::fromNodo(1000000)
        );

    requireCondition(
        cap.rawUnits() == 10958904109LL,
        "Inflation emission cap calculation is wrong."
    );
}

void testProtectionEpochRewardPool() {
    const ProtectionEpoch epoch(
        1,
        0,
        10,
        Amount::fromNodo(25),
        Amount::fromNodo(100),
        7000
    );

    requireCondition(
        epoch.isValid(),
        "Protection epoch should be valid."
    );

    requireCondition(
        epoch.securityEmission() == Amount::fromNodo(70),
        "Protection epoch security emission is wrong."
    );

    requireCondition(
        epoch.rewardPool() == Amount::fromNodo(95),
        "Protection epoch reward pool is wrong."
    );
}

void testValidationWorkRecordRewardContribution() {
    const ValidationWorkRecord acceptedWork(
        "nodo1validator",
        2,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::ACCEPTED,
        "target-hash",
        "evidence-hash",
        10,
        kTimestamp
    );

    const ValidationWorkRecord rejectedWork(
        "nodo1validator",
        2,
        ValidationWorkType::VERIFY_COIN_EXISTENCE,
        ValidationWorkResult::REJECTED,
        "target-hash",
        "evidence-hash",
        10,
        kTimestamp
    );

    requireCondition(
        acceptedWork.isValid(),
        "Accepted validation work should be structurally valid."
    );

    requireCondition(
        acceptedWork.contributesToReward(),
        "Accepted validation work should contribute to reward."
    );

    requireCondition(
        rejectedWork.isValid(),
        "Rejected validation work should still be structurally valid."
    );

    requireCondition(
        !rejectedWork.contributesToReward(),
        "Rejected validation work must not contribute to reward."
    );
}

void testValidatorScoreIsBoundedTrust() {
    const ValidatorScoreRecord scoreRecord(
        "nodo1validator",
        3,
        70,
        75,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence",
        kTimestamp
    );

    requireCondition(
        scoreRecord.isValid(),
        "Validator score record should be valid."
    );

    requireCondition(
        scoreRecord.scoreDelta() == 5,
        "Validator score delta is wrong."
    );

    requireCondition(
        scoreRecord.trustFactorBasisPoints() == 7500,
        "Validator trust factor basis points are wrong."
    );

    const ValidatorScoreRecord invalidScore(
        "nodo1validator",
        3,
        70,
        101,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence",
        kTimestamp
    );

    requireCondition(
        !invalidScore.isValid(),
        "Validator score above 100 should be invalid."
    );
}

void testGenesisRewardCreatesTraceableCoinLot() {
    const GenesisRewardRecord reward(
        4,
        "nodo1validator",
        Amount::fromNodo(12),
        GenesisRewardReason::NETWORK_PROTECTION,
        "work-summary-hash",
        "NODO_EPOCH_EMISSION_POLICY_V1",
        "accepted-block-hash",
        kTimestamp
    );

    requireCondition(
        reward.isValid(),
        "Genesis reward record should be valid."
    );

    const std::string firstId =
        reward.deterministicId();

    const std::string secondId =
        reward.deterministicId();

    requireCondition(
        firstId == secondId,
        "Genesis reward deterministic id is not deterministic."
    );

    requireCondition(
        firstId.size() == 64,
        "Genesis reward id should be SHA-256 hex."
    );

    const CoinLot lot =
        reward.createRewardCoinLot(10);

    requireCondition(
        lot.isValid(),
        "Genesis reward coin lot should be valid."
    );

    requireCondition(
        lot.originMintRecordId() == firstId,
        "Reward coin lot should reference the GenesisReward id."
    );

    requireCondition(
        lot.ownerAddress() == reward.validatorAddress(),
        "Reward coin lot owner should be the validator."
    );

    requireCondition(
        lot.amount() == reward.amount(),
        "Reward coin lot amount should match reward amount."
    );

    requireCondition(
        lot.isAvailable(),
        "Reward coin lot should be available after creation."
    );
}

} // namespace

int main() {
    try {
        testEmissionPolicyUsesBootstrapWhenSupplyIsZero();
        testEmissionPolicyCalculatesInflationCap();
        testProtectionEpochRewardPool();
        testValidationWorkRecordRewardContribution();
        testValidatorScoreIsBoundedTrust();
        testGenesisRewardCreatesTraceableCoinLot();

        std::cout << "Nodo protection economics tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protection economics tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
