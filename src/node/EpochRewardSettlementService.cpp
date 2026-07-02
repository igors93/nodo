#include "node/EpochRewardSettlementService.hpp"

#include "economics/EpochEmissionPolicy.hpp"
#include "economics/EpochRewardLedgerBuilder.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "node/EpochParticipation.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <sstream>
#include <stdexcept>
#include <limits>
#include <utility>

namespace nodo::node {

namespace {

bool isEpochRecordType(core::LedgerRecordType type) {
    return type == core::LedgerRecordType::VALIDATION_WORK ||
           type == core::LedgerRecordType::VALIDATOR_SCORE ||
           type == core::LedgerRecordType::PROTECTION_EPOCH ||
           type == core::LedgerRecordType::GENESIS_REWARD;
}

utils::Amount checkedTotalMinted(
    const std::vector<economics::GenesisRewardRecord>& records
) {
    utils::Amount total;
    for (const auto& record : records) {
        total = total + record.amount();
    }
    return total;
}

bool sameRecords(
    const std::vector<core::LedgerRecord>& left,
    const std::vector<core::LedgerRecord>& right
) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) return false;
    }
    return true;
}

} // namespace

EpochRewardSettlement::EpochRewardSettlement()
    : m_distribution(), m_canonicalRecords(), m_rewardDistributions(),
      m_updatedAccounts(), m_totalMinted(), m_settledAtBlock(0) {}

EpochRewardSettlement::EpochRewardSettlement(
    economics::EpochRewardDistribution distribution,
    std::vector<core::LedgerRecord> canonicalRecords,
    std::vector<RewardDistribution> rewardDistributions,
    core::AccountStateView updatedAccounts,
    utils::Amount totalMinted,
    std::uint64_t settledAtBlock
) : m_distribution(std::move(distribution)),
    m_canonicalRecords(std::move(canonicalRecords)),
    m_rewardDistributions(std::move(rewardDistributions)),
    m_updatedAccounts(std::move(updatedAccounts)),
    m_totalMinted(totalMinted),
    m_settledAtBlock(settledAtBlock) {}

const economics::EpochRewardDistribution& EpochRewardSettlement::distribution() const { return m_distribution; }
const std::vector<core::LedgerRecord>& EpochRewardSettlement::canonicalRecords() const { return m_canonicalRecords; }
const std::vector<RewardDistribution>& EpochRewardSettlement::rewardDistributions() const { return m_rewardDistributions; }
const core::AccountStateView& EpochRewardSettlement::updatedAccounts() const { return m_updatedAccounts; }
utils::Amount EpochRewardSettlement::totalMinted() const { return m_totalMinted; }
std::uint64_t EpochRewardSettlement::settledAtBlock() const { return m_settledAtBlock; }

bool EpochRewardSettlement::isValid() const {
    if (m_settledAtBlock == 0 || !m_distribution.isValid() ||
        !m_updatedAccounts.isValid() || m_totalMinted.isNegative()) {
        return false;
    }
    const std::size_t rewardLedgerSize =
        m_distribution.genesisRewardRecords().size() + 1U;
    if (m_canonicalRecords.size() < rewardLedgerSize ||
        m_totalMinted != m_distribution.distributedSecurityEmission() ||
        checkedTotalMinted(m_distribution.genesisRewardRecords()) != m_totalMinted ||
        !economics::EpochRewardLedgerBuilder::recordsMatchDistribution(
            m_distribution,
            std::vector<core::LedgerRecord>(
                m_canonicalRecords.end() - static_cast<std::ptrdiff_t>(
                    rewardLedgerSize
                ),
                m_canonicalRecords.end()
            )
        )) {
        return false;
    }
    if (m_rewardDistributions.size() != m_distribution.genesisRewardRecords().size()) {
        return false;
    }
    for (const auto& reward : m_rewardDistributions) {
        if (!reward.isValid() || reward.blockHeight() != m_settledAtBlock ||
            reward.reason() != RewardDistributionCalculator::EPOCH_PROTECTION_REWARD_REASON) {
            return false;
        }
    }
    return true;
}

std::string EpochRewardSettlement::serialize() const {
    std::ostringstream output;
    output << "EpochRewardSettlement{settledAtBlock=" << m_settledAtBlock
           << ";epoch=" << m_distribution.protectionEpoch().epochId()
           << ";recordCount=" << m_canonicalRecords.size()
           << ";beneficiaryCount=" << m_rewardDistributions.size()
           << ";totalMintedRaw=" << m_totalMinted.rawUnits() << '}';
    return output.str();
}

bool EpochRewardSettlementService::isSettlementHeight(
    std::uint64_t candidateHeight
) {
    return candidateHeight > 1 &&
           (candidateHeight - 1U) % NODO_VALIDATOR_EPOCH_BLOCKS == 0;
}

std::uint64_t EpochRewardSettlementService::settledEpochForHeight(
    std::uint64_t candidateHeight
) {
    if (!isSettlementHeight(candidateHeight)) {
        throw std::invalid_argument("Block height is not an epoch settlement boundary.");
    }
    return (candidateHeight - 1U) / NODO_VALIDATOR_EPOCH_BLOCKS;
}

core::AccountStateView EpochRewardSettlementService::applyDistributions(
    const std::vector<RewardDistribution>& distributions,
    const core::AccountStateView& accounts
) {
    core::AccountStateView updated = accounts;
    for (const auto& distribution : distributions) {
        if (!distribution.isValid() || distribution.liquidReward().isNegative()) {
            throw std::invalid_argument("Invalid epoch reward distribution rejected.");
        }
        if (distribution.liquidReward().isZero()) continue;
        const core::AccountState current =
            updated.accountOrDefault(distribution.validatorAddress());
        if (!updated.putAccount(core::AccountState(
                distribution.validatorAddress(),
                current.balance() + distribution.liquidReward(),
                current.nonce()))) {
            throw std::logic_error("Epoch reward credit produced an invalid account.");
        }
    }
    return updated;
}

EpochRewardSettlement EpochRewardSettlementService::buildForCandidate(
    const NodeRuntime& runtime,
    std::uint64_t candidateHeight,
    std::int64_t candidateTimestamp
) {
    if (!isSettlementHeight(candidateHeight) || candidateTimestamp <= 0 ||
        runtime.blockchain().empty() ||
        runtime.blockchain().latestBlock().index() + 1U != candidateHeight) {
        throw std::invalid_argument("Invalid runtime boundary for epoch reward settlement.");
    }
    return buildFromFinalizedHistory(
        runtime, candidateHeight, candidateTimestamp,
        runtime.supplyState().latestSupply()
    );
}

EpochRewardSettlement EpochRewardSettlementService::buildFromFinalizedHistory(
    const NodeRuntime& runtime,
    std::uint64_t candidateHeight,
    std::int64_t candidateTimestamp,
    utils::Amount supplyBefore
) {
    if (!isSettlementHeight(candidateHeight) || candidateTimestamp <= 0 ||
        supplyBefore.isNegative()) {
        throw std::invalid_argument("Invalid finalized history reward context.");
    }
    const std::uint64_t epoch = settledEpochForHeight(candidateHeight);
    const std::uint64_t startBlock = ValidatorLifecycle::epochStartBlock(epoch);
    const std::uint64_t endBlock = ValidatorLifecycle::epochEndBlock(epoch);
    const EpochParticipationSnapshot participation = EpochParticipation::build(
        epoch, startBlock, endBlock,
        runtime.config().genesisConfig().networkParameters().chainId(),
        runtime.blockchain(), runtime.validatorSetHistory(),
        runtime.finalizationRegistry(), candidateTimestamp
    );
    const economics::EpochRewardDistribution distribution =
        economics::EpochRewardDistributor::distribute(
            epoch, startBlock, endBlock,
            supplyBefore, utils::Amount(),
            participation.targetWorkWeight(),
            economics::EpochEmissionPolicy::developmentDefaultPolicy(),
            participation.workRecords(), participation.scoreRecords(),
            runtime.blockchain().blocks().at(endBlock).hash(), candidateTimestamp
        );

    std::vector<core::LedgerRecord> canonical =
        participation.ledgerRecords(candidateTimestamp);
    const auto rewardLedger = economics::EpochRewardLedgerBuilder::buildLedgerRecords(
        distribution, candidateTimestamp
    );
    canonical.insert(canonical.end(), rewardLedger.records().begin(), rewardLedger.records().end());
    const auto rewardDistributions = RewardDistributionCalculator::buildFromEpochRewards(
        distribution.genesisRewardRecords(), candidateHeight
    );
    const std::uint64_t minimumFee = runtime.effectiveMinimumFeeRawUnits();
    if (minimumFee > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Effective minimum fee exceeds Amount range.");
    }
    EpochRewardSettlement settlement(
        distribution, std::move(canonical), rewardDistributions,
        applyDistributions(
            rewardDistributions,
            runtime.cachedAccountStateAtTip(static_cast<std::int64_t>(minimumFee))
        ),
        distribution.distributedSecurityEmission(), candidateHeight
    );
    if (!settlement.isValid()) {
        throw std::logic_error("Epoch reward settlement builder produced an invalid result.");
    }
    return settlement;
}

std::vector<core::LedgerRecord> EpochRewardSettlementService::epochRecords(
    const std::vector<core::LedgerRecord>& records
) {
    std::vector<core::LedgerRecord> result;
    for (const auto& record : records) {
        if (isEpochRecordType(record.type())) result.push_back(record);
    }
    return result;
}

bool EpochRewardSettlementService::candidateRecordsMatch(
    const NodeRuntime& runtime,
    const core::Block& candidate,
    std::string& rejectionReason
) {
    try {
        const std::vector<core::LedgerRecord> actual = epochRecords(candidate.records());
        if (!isSettlementHeight(candidate.index())) {
            if (!actual.empty()) {
                rejectionReason = "Epoch reward records are only allowed at a settlement boundary.";
                return false;
            }
            rejectionReason.clear();
            return true;
        }
        const EpochRewardSettlement expected = buildForCandidate(
            runtime, candidate.index(), candidate.timestamp()
        );
        if (!sameRecords(actual, expected.canonicalRecords())) {
            rejectionReason = "Candidate epoch reward records do not match finalized participation evidence.";
            return false;
        }
        rejectionReason.clear();
        return true;
    } catch (const std::exception& error) {
        rejectionReason = error.what();
        return false;
    }
}

bool EpochRewardSettlementService::finalizedRecordsMatch(
    const NodeRuntime& runtime,
    const core::Block& finalizedBlock,
    utils::Amount supplyBefore,
    std::string& rejectionReason
) {
    try {
        const std::vector<core::LedgerRecord> actual = epochRecords(finalizedBlock.records());
        if (!isSettlementHeight(finalizedBlock.index())) {
            if (!actual.empty()) {
                rejectionReason = "Epoch reward records exist outside a settlement boundary.";
                return false;
            }
            rejectionReason.clear();
            return true;
        }
        const EpochRewardSettlement expected = buildFromFinalizedHistory(
            runtime, finalizedBlock.index(), finalizedBlock.timestamp(), supplyBefore
        );
        if (!sameRecords(actual, expected.canonicalRecords())) {
            rejectionReason = "Finalized epoch rewards do not match quorum participation history.";
            return false;
        }
        rejectionReason.clear();
        return true;
    } catch (const std::exception& error) {
        rejectionReason = error.what();
        return false;
    }
}

EpochRewardSettlement EpochRewardSettlementService::settleCanonicalRecords(
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp,
    const std::vector<core::LedgerRecord>& records,
    utils::Amount supplyBefore,
    const core::AccountStateView& currentAccounts
) {
    if (!isSettlementHeight(blockHeight)) {
        if (!epochRecords(records).empty()) {
            throw std::invalid_argument("Unexpected epoch reward records outside settlement boundary.");
        }
        return EpochRewardSettlement();
    }

    const std::vector<core::LedgerRecord> canonical = epochRecords(records);
    std::vector<economics::ValidationWorkRecord> work;
    std::vector<economics::ValidatorScoreRecord> scores;
    std::vector<core::LedgerRecord> rewardLedger;
    economics::ProtectionEpoch epochRecord;
    bool foundEpoch = false;
    bool reachedEpochSummary = false;

    for (const auto& record : canonical) {
        if (record.timestamp() != blockTimestamp) {
            throw std::invalid_argument("Epoch ledger timestamp does not match settlement block.");
        }
        if (record.type() == core::LedgerRecordType::VALIDATION_WORK && !reachedEpochSummary) {
            const auto parsed = economics::ValidationWorkRecord::deserialize(record.payload());
            if (parsed.timestamp() != blockTimestamp) {
                throw std::invalid_argument("Validation work timestamp does not match settlement block.");
            }
            work.push_back(parsed);
        } else if (record.type() == core::LedgerRecordType::VALIDATOR_SCORE && !reachedEpochSummary) {
            const auto parsed = economics::ValidatorScoreRecord::deserialize(record.payload());
            if (parsed.timestamp() != blockTimestamp) {
                throw std::invalid_argument("Validator score timestamp does not match settlement block.");
            }
            scores.push_back(parsed);
        } else if (record.type() == core::LedgerRecordType::PROTECTION_EPOCH && !foundEpoch) {
            reachedEpochSummary = true;
            foundEpoch = true;
            epochRecord = economics::ProtectionEpoch::deserialize(record.payload());
            rewardLedger.push_back(record);
        } else if (record.type() == core::LedgerRecordType::GENESIS_REWARD && foundEpoch) {
            rewardLedger.push_back(record);
        } else {
            throw std::invalid_argument("Epoch reward records are not in canonical order.");
        }
    }

    const std::uint64_t epoch = settledEpochForHeight(blockHeight);
    if (!foundEpoch || !epochRecord.hasCanonicalSettlementMetadata() ||
        epochRecord.epochId() != epoch ||
        epochRecord.startBlock() != ValidatorLifecycle::epochStartBlock(epoch) ||
        epochRecord.endBlock() != ValidatorLifecycle::epochEndBlock(epoch) ||
        epochRecord.policyVersion() !=
            economics::EpochEmissionPolicy::developmentDefaultPolicy().policyVersion() ||
        epochRecord.feesCollected() != utils::Amount()) {
        throw std::invalid_argument("Epoch settlement metadata is missing or inconsistent.");
    }
    const economics::EpochRewardDistribution distribution =
        economics::EpochRewardDistributor::distribute(
            epoch, epochRecord.startBlock(), epochRecord.endBlock(),
            supplyBefore, epochRecord.feesCollected(), epochRecord.targetWorkWeight(),
            economics::EpochEmissionPolicy::developmentDefaultPolicy(),
            work, scores, epochRecord.evidenceBlockHash(), blockTimestamp
        );
    if (!economics::EpochRewardLedgerBuilder::recordsMatchDistribution(
            distribution, rewardLedger)) {
        throw std::invalid_argument("Genesis reward records do not match canonical epoch distribution.");
    }

    const auto rewardDistributions = RewardDistributionCalculator::buildFromEpochRewards(
        distribution.genesisRewardRecords(), blockHeight
    );
    EpochRewardSettlement settlement(
        distribution, canonical, rewardDistributions,
        applyDistributions(rewardDistributions, currentAccounts),
        distribution.distributedSecurityEmission(), blockHeight
    );
    if (!settlement.isValid()) {
        throw std::logic_error("Canonical epoch reward settlement is invalid.");
    }
    return settlement;
}

} // namespace nodo::node
