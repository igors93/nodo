#include "core/LedgerRecord.hpp"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardDistributor.hpp"
#include "economics/EpochRewardLedgerBuilder.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "node/EpochRewardSettlementService.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<core::LedgerRecord> canonicalRecords() {
    const economics::ValidationWorkRecord work(
        "validator-a", 1, economics::ValidationWorkType::CONSENSUS_VOTE,
        economics::ValidationWorkResult::ACCEPTED, "epoch-end-hash",
        "epoch-vote-evidence", 100, kTimestamp
    );
    const economics::ValidatorScoreRecord score(
        "validator-a", 1, 0, 100,
        economics::ValidatorScoreReason::CONSISTENT_VALIDATION,
        "epoch-uptime-evidence", kTimestamp
    );
    const economics::EpochRewardDistribution distribution =
        economics::EpochRewardDistributor::distribute(
            1, 1, 43200, utils::Amount::fromNodo(36500), utils::Amount(), 100,
            economics::EpochEmissionPolicy::developmentDefaultPolicy(),
            {work}, {score}, "epoch-end-hash", kTimestamp
        );
    std::vector<core::LedgerRecord> records = {
        core::LedgerRecord::fromValidationWorkRecord(work, kTimestamp),
        core::LedgerRecord::fromValidatorScoreRecord(score, kTimestamp)
    };
    const auto rewardRecords = economics::EpochRewardLedgerBuilder::buildLedgerRecords(
        distribution, kTimestamp
    );
    records.insert(records.end(), rewardRecords.records().begin(), rewardRecords.records().end());
    return records;
}

void testCanonicalBundleCreditsAccountAndSupply() {
    const node::EpochRewardSettlement settlement =
        node::EpochRewardSettlementService::settleCanonicalRecords(
            43201, kTimestamp, canonicalRecords(),
            utils::Amount::fromNodo(36500), core::AccountStateView()
        );
    requireCondition(settlement.isValid(), "Canonical epoch settlement should be valid.");
    requireCondition(settlement.totalMinted() == utils::Amount::fromNodo(4),
        "Epoch emission must be bounded to one 4%-annualized epoch.");
    requireCondition(
        settlement.updatedAccounts().accountOrDefault("validator-a").balance() ==
            settlement.totalMinted(),
        "Canonical GenesisReward must credit the rewarded validator account."
    );
}

void testIncompleteBundleIsRejected() {
    std::vector<core::LedgerRecord> records = canonicalRecords();
    records.pop_back();
    bool rejected = false;
    try {
        (void)node::EpochRewardSettlementService::settleCanonicalRecords(
            43201, kTimestamp, records,
            utils::Amount::fromNodo(36500), core::AccountStateView()
        );
    } catch (const std::exception&) {
        rejected = true;
    }
    requireCondition(rejected, "Incomplete epoch reward bundles must be rejected.");
}

} // namespace

int main() {
    try {
        testCanonicalBundleCreditsAccountAndSupply();
        testIncompleteBundleIsRejected();
        std::cout << "Epoch reward settlement service tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Epoch reward settlement service tests failed: " << error.what() << '\n';
        return 1;
    }
}
