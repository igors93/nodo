#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ProtectionBlockProposal.hpp"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardDistributor.hpp"
#include "economics/EpochRewardLedgerBuilder.hpp"
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
using nodo::core::LedgerRecordType;
using nodo::core::ProtectionBlockBuilder;
using nodo::core::ProtectionBlockProposal;
using nodo::economics::EpochEmissionPolicy;
using nodo::economics::EpochRewardDistribution;
using nodo::economics::EpochRewardDistributor;
using nodo::economics::EpochRewardLedgerBuildResult;
using nodo::economics::EpochRewardLedgerBuilder;
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

ValidationWorkRecord work(
    const std::string& validator,
    std::uint64_t epoch,
    std::uint32_t weight,
    const std::string& evidenceHash
) {
    return ValidationWorkRecord(
        validator,
        epoch,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "target-" + evidenceHash,
        evidenceHash,
        weight,
        kTimestamp
    );
}

ValidatorScoreRecord score(
    const std::string& validator,
    std::uint64_t epoch,
    std::int32_t newScore
) {
    return ValidatorScoreRecord(
        validator,
        epoch,
        50,
        newScore,
        ValidatorScoreReason::CONSISTENT_VALIDATION,
        "score-evidence-" + validator,
        kTimestamp
    );
}

Blockchain baseBlockchain(
    std::int64_t timestampOffset = 0
) {
    const ValidationWorkRecord genesisWork =
        work(
            "nodo1bootstrap",
            1,
            1,
            "bootstrap-evidence-" + std::to_string(timestampOffset)
        );

    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    genesisWork,
                    kTimestamp + timestampOffset
                )
            },
            kTimestamp + timestampOffset + 1
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);

    return blockchain;
}

EpochRewardDistribution rewardDistribution(
    std::uint64_t epochId = 1
) {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", epochId, 60, "proposal-evidence-a"),
        work("nodo1validatorB", epochId, 40, "proposal-evidence-b")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {
        score("nodo1validatorA", epochId, 100),
        score("nodo1validatorB", epochId, 50)
    };

    return EpochRewardDistributor::distribute(
        epochId,
        10,
        20,
        Amount::fromNodo(36500),
        Amount::fromNodo(3),
        100,
        policy,
        workRecords,
        scoreRecords,
        "accepted-block-hash-for-proposal",
        kTimestamp + 10
    );
}

EpochRewardDistribution emptyRewardDistribution() {
    const EpochEmissionPolicy policy =
        EpochEmissionPolicy::developmentDefaultPolicy();

    const std::vector<ValidationWorkRecord> workRecords = {
        work("nodo1validatorA", 1, 100, "no-score-evidence")
    };

    const std::vector<ValidatorScoreRecord> scoreRecords = {};

    return EpochRewardDistributor::distribute(
        1,
        10,
        20,
        Amount::fromNodo(36500),
        Amount::fromRawUnits(0),
        100,
        policy,
        workRecords,
        scoreRecords,
        "accepted-block-hash-no-score",
        kTimestamp + 10
    );
}

void testLedgerBuilderCreatesCanonicalRecords() {
    const EpochRewardDistribution distribution =
        rewardDistribution();

    const EpochRewardLedgerBuildResult result =
        EpochRewardLedgerBuilder::buildLedgerRecords(
            distribution,
            kTimestamp + 20
        );

    requireCondition(
        result.isValid(),
        "Reward ledger build result should be valid."
    );

    requireCondition(
        result.recordCount() == 3U,
        "Reward ledger build should contain epoch plus two rewards."
    );

    requireCondition(
        result.hasProtectionEpochRecord(),
        "Reward ledger build should include ProtectionEpoch record."
    );

    requireCondition(
        result.records().front().type() == LedgerRecordType::PROTECTION_EPOCH,
        "ProtectionEpoch record must be first."
    );

    requireCondition(
        result.records()[1].type() == LedgerRecordType::GENESIS_REWARD,
        "GenesisReward records should follow ProtectionEpoch."
    );

    requireCondition(
        EpochRewardLedgerBuilder::recordsMatchDistribution(
            distribution,
            result.records()
        ),
        "Reward ledger records should match distribution."
    );
}

void testBlockBuilderCreatesAppendableRewardBlockProposal() {
    Blockchain blockchain =
        baseBlockchain();

    const EpochRewardDistribution distribution =
        rewardDistribution();

    const ProtectionBlockProposal proposal =
        ProtectionBlockBuilder::buildRewardBlockProposal(
            blockchain,
            distribution,
            kTimestamp + 30
        );

    requireCondition(
        proposal.isValidForBlockchain(blockchain),
        "Protection block proposal should be valid for current chain."
    );

    requireCondition(
        proposal.block().index() == blockchain.size(),
        "Protection block proposal index should be next chain index."
    );

    requireCondition(
        proposal.block().previousHash() == blockchain.latestBlock().hash(),
        "Protection block proposal previous hash mismatch."
    );

    requireCondition(
        proposal.block().records().size() == 3U,
        "Reward block should include one epoch record and two reward records."
    );

    proposal.appendToBlockchain(blockchain);

    requireCondition(
        blockchain.size() == 2U,
        "Appending reward block should increase chain size."
    );

    requireCondition(
        blockchain.isValid(),
        "Blockchain should remain valid after reward block append."
    );

    requireCondition(
        blockchain.latestBlock().records().front().type() == LedgerRecordType::PROTECTION_EPOCH,
        "Latest block should start with ProtectionEpoch record."
    );
}

void testProposalCannotBeReusedOnDifferentChainTip() {
    Blockchain originalChain =
        baseBlockchain(0);

    Blockchain otherChain =
        baseBlockchain(100);

    const ProtectionBlockProposal proposal =
        ProtectionBlockBuilder::buildRewardBlockProposal(
            originalChain,
            rewardDistribution(),
            kTimestamp + 40
        );

    requireCondition(
        proposal.isValidForBlockchain(originalChain),
        "Proposal should be valid for original chain."
    );

    requireCondition(
        !proposal.isValidForBlockchain(otherChain),
        "Proposal should not be valid for a different chain tip."
    );
}

void testNoRewardDistributionStillCreatesAuditableEpochBlock() {
    Blockchain blockchain =
        baseBlockchain();

    const EpochRewardDistribution distribution =
        emptyRewardDistribution();

    requireCondition(
        distribution.validatorRewards().empty(),
        "Fixture should have no reward records."
    );

    const ProtectionBlockProposal proposal =
        ProtectionBlockBuilder::buildRewardBlockProposal(
            blockchain,
            distribution,
            kTimestamp + 50
        );

    requireCondition(
        proposal.isValidForBlockchain(blockchain),
        "No-reward epoch proposal should still be valid."
    );

    requireCondition(
        proposal.block().records().size() == 1U,
        "No-reward epoch block should contain only ProtectionEpoch."
    );

    requireCondition(
        proposal.block().records().front().type() == LedgerRecordType::PROTECTION_EPOCH,
        "No-reward epoch block should contain ProtectionEpoch record."
    );
}

void testInvalidProposalInputsAreRejected() {
    bool rejected = false;

    try {
        (void)ProtectionBlockBuilder::buildRewardBlockProposal(
            Blockchain(),
            rewardDistribution(),
            kTimestamp + 60
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Empty blockchain should reject reward block proposal."
    );

    rejected = false;

    try {
        (void)ProtectionBlockBuilder::buildRewardBlockProposal(
            baseBlockchain(),
            rewardDistribution(),
            0
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Zero proposal timestamp should be rejected."
    );
}

} // namespace

int main() {
    try {
        testLedgerBuilderCreatesCanonicalRecords();
        testBlockBuilderCreatesAppendableRewardBlockProposal();
        testProposalCannotBeReusedOnDifferentChainTip();
        testNoRewardDistributionStillCreatesAuditableEpochBlock();
        testInvalidProposalInputsAreRejected();

        std::cout << "Nodo epoch reward block proposal tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo epoch reward block proposal tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
