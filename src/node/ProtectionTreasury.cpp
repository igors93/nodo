#include "node/ProtectionTreasury.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

utils::Amount minAmount(
    utils::Amount left,
    utils::Amount right
) {
    return left <= right ? left : right;
}

std::int64_t basisPointAmount(
    std::int64_t rawUnits,
    std::int64_t basisPoints
) {
    if (rawUnits <= 0 || basisPoints <= 0) {
        return 0;
    }

    const std::int64_t whole =
        rawUnits / 10000;

    const std::int64_t remainder =
        rawUnits % 10000;

    if (whole > std::numeric_limits<std::int64_t>::max() / basisPoints) {
        return std::numeric_limits<std::int64_t>::max();
    }

    const std::int64_t wholePart =
        whole * basisPoints;

    const std::int64_t remainderPart =
        (remainder * basisPoints) / 10000;

    if (wholePart > std::numeric_limits<std::int64_t>::max() - remainderPart) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return wholePart + remainderPart;
}

std::uint16_t scoreForValidator(
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::string& validatorAddress
) {
    for (const SecurityScoreRecord& record : securityScoreRecords) {
        if (record.validatorAddress() == validatorAddress) {
            return record.score();
        }
    }

    return SECURITY_SCORE_MIN;
}

} // namespace

GenesisTreasurySnapshot::GenesisTreasurySnapshot()
    : m_status("NOT_EVALUATED"),
      m_treasuryAddress(""),
      m_blockHeight(0),
      m_genesisTreasuryBalance(),
      m_protectedReserve(),
      m_protectionBudget(),
      m_availableBalance(),
      m_reason(ProtectionTreasury::NOT_EVALUATED_REASON) {}

GenesisTreasurySnapshot::GenesisTreasurySnapshot(
    std::string status,
    std::string treasuryAddress,
    std::uint64_t blockHeight,
    utils::Amount genesisTreasuryBalance,
    utils::Amount protectedReserve,
    utils::Amount protectionBudget,
    utils::Amount availableBalance,
    std::string reason
)
    : m_status(std::move(status)),
      m_treasuryAddress(std::move(treasuryAddress)),
      m_blockHeight(blockHeight),
      m_genesisTreasuryBalance(genesisTreasuryBalance),
      m_protectedReserve(protectedReserve),
      m_protectionBudget(protectionBudget),
      m_availableBalance(availableBalance),
      m_reason(std::move(reason)) {}

GenesisTreasurySnapshot GenesisTreasurySnapshot::notEvaluated() {
    return GenesisTreasurySnapshot();
}

const std::string& GenesisTreasurySnapshot::status() const {
    return m_status;
}

const std::string& GenesisTreasurySnapshot::treasuryAddress() const {
    return m_treasuryAddress;
}

std::uint64_t GenesisTreasurySnapshot::blockHeight() const {
    return m_blockHeight;
}

utils::Amount GenesisTreasurySnapshot::genesisTreasuryBalance() const {
    return m_genesisTreasuryBalance;
}

utils::Amount GenesisTreasurySnapshot::protectedReserve() const {
    return m_protectedReserve;
}

utils::Amount GenesisTreasurySnapshot::protectionBudget() const {
    return m_protectionBudget;
}

utils::Amount GenesisTreasurySnapshot::availableBalance() const {
    return m_availableBalance;
}

const std::string& GenesisTreasurySnapshot::reason() const {
    return m_reason;
}

bool GenesisTreasurySnapshot::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool GenesisTreasurySnapshot::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == ProtectionTreasury::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_treasuryAddress != ProtectionTreasury::TREASURY_ADDRESS ||
        m_blockHeight == 0 ||
        m_genesisTreasuryBalance.isNegative() ||
        m_protectedReserve.isNegative() ||
        m_protectionBudget.isNegative() ||
        m_availableBalance.isNegative() ||
        m_reason != ProtectionTreasury::TREASURY_SNAPSHOT_REASON) {
        return false;
    }

    return m_protectedReserve + m_protectionBudget == m_genesisTreasuryBalance &&
           m_availableBalance == m_genesisTreasuryBalance;
}

std::string GenesisTreasurySnapshot::serialize() const {
    std::ostringstream oss;

    oss << "GenesisTreasurySnapshot{"
        << "status=" << m_status
        << ";treasuryAddress=" << m_treasuryAddress
        << ";blockHeight=" << m_blockHeight
        << ";genesisTreasuryBalanceRawUnits=" << m_genesisTreasuryBalance.rawUnits()
        << ";protectedReserveRawUnits=" << m_protectedReserve.rawUnits()
        << ";protectionBudgetRawUnits=" << m_protectionBudget.rawUnits()
        << ";availableBalanceRawUnits=" << m_availableBalance.rawUnits()
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

ProtectionRewardBudget::ProtectionRewardBudget()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_treasuryAddress(""),
      m_availableBudget(),
      m_plannedTotal(),
      m_remainingBudget(),
      m_beneficiaryCount(0),
      m_reason(ProtectionTreasury::NOT_EVALUATED_REASON),
      m_sourceTreasuryDigest("") {}

ProtectionRewardBudget::ProtectionRewardBudget(
    std::string status,
    std::uint64_t blockHeight,
    std::string treasuryAddress,
    utils::Amount availableBudget,
    utils::Amount plannedTotal,
    utils::Amount remainingBudget,
    std::uint64_t beneficiaryCount,
    std::string reason,
    std::string sourceTreasuryDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_treasuryAddress(std::move(treasuryAddress)),
      m_availableBudget(availableBudget),
      m_plannedTotal(plannedTotal),
      m_remainingBudget(remainingBudget),
      m_beneficiaryCount(beneficiaryCount),
      m_reason(std::move(reason)),
      m_sourceTreasuryDigest(std::move(sourceTreasuryDigest)) {}

ProtectionRewardBudget ProtectionRewardBudget::notEvaluated() {
    return ProtectionRewardBudget();
}

const std::string& ProtectionRewardBudget::status() const {
    return m_status;
}

std::uint64_t ProtectionRewardBudget::blockHeight() const {
    return m_blockHeight;
}

const std::string& ProtectionRewardBudget::treasuryAddress() const {
    return m_treasuryAddress;
}

utils::Amount ProtectionRewardBudget::availableBudget() const {
    return m_availableBudget;
}

utils::Amount ProtectionRewardBudget::plannedTotal() const {
    return m_plannedTotal;
}

utils::Amount ProtectionRewardBudget::remainingBudget() const {
    return m_remainingBudget;
}

std::uint64_t ProtectionRewardBudget::beneficiaryCount() const {
    return m_beneficiaryCount;
}

const std::string& ProtectionRewardBudget::reason() const {
    return m_reason;
}

const std::string& ProtectionRewardBudget::sourceTreasuryDigest() const {
    return m_sourceTreasuryDigest;
}

bool ProtectionRewardBudget::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool ProtectionRewardBudget::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == ProtectionTreasury::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_treasuryAddress != ProtectionTreasury::TREASURY_ADDRESS ||
        m_availableBudget.isNegative() ||
        m_plannedTotal.isNegative() ||
        m_remainingBudget.isNegative() ||
        m_plannedTotal > m_availableBudget ||
        m_remainingBudget != m_availableBudget - m_plannedTotal ||
        m_reason != ProtectionTreasury::PROTECTION_BUDGET_REASON ||
        m_sourceTreasuryDigest.empty()) {
        return false;
    }

    if (m_plannedTotal.isZero()) {
        return m_beneficiaryCount == 0;
    }

    return m_beneficiaryCount > 0;
}

std::string ProtectionRewardBudget::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionRewardBudget{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";treasuryAddress=" << m_treasuryAddress
        << ";availableBudgetRawUnits=" << m_availableBudget.rawUnits()
        << ";plannedTotalRawUnits=" << m_plannedTotal.rawUnits()
        << ";remainingBudgetRawUnits=" << m_remainingBudget.rawUnits()
        << ";beneficiaryCount=" << m_beneficiaryCount
        << ";reason=" << m_reason
        << ";sourceTreasuryDigest=" << m_sourceTreasuryDigest
        << "}";

    return oss.str();
}

ProtectionRewardGrant::ProtectionRewardGrant()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_plannedReward(),
      m_securityScore(0),
      m_reason(""),
      m_sourceBudgetDigest("") {}

ProtectionRewardGrant::ProtectionRewardGrant(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    utils::Amount plannedReward,
    std::uint16_t securityScore,
    std::string reason,
    std::string sourceBudgetDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_plannedReward(plannedReward),
      m_securityScore(securityScore),
      m_reason(std::move(reason)),
      m_sourceBudgetDigest(std::move(sourceBudgetDigest)) {}

const std::string& ProtectionRewardGrant::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ProtectionRewardGrant::blockHeight() const {
    return m_blockHeight;
}

utils::Amount ProtectionRewardGrant::plannedReward() const {
    return m_plannedReward;
}

std::uint16_t ProtectionRewardGrant::securityScore() const {
    return m_securityScore;
}

const std::string& ProtectionRewardGrant::reason() const {
    return m_reason;
}

const std::string& ProtectionRewardGrant::sourceBudgetDigest() const {
    return m_sourceBudgetDigest;
}

bool ProtectionRewardGrant::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           !m_plannedReward.isNegative() &&
           m_plannedReward.isPositive() &&
           m_securityScore >= SECURITY_SCORE_MIN &&
           m_securityScore <= SECURITY_SCORE_MAX &&
           m_reason == ProtectionTreasury::PROTECTION_GRANT_REASON &&
           !m_sourceBudgetDigest.empty();
}

std::string ProtectionRewardGrant::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionRewardGrant{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";plannedRewardRawUnits=" << m_plannedReward.rawUnits()
        << ";securityScore=" << m_securityScore
        << ";reason=" << m_reason
        << ";sourceBudgetDigest=" << m_sourceBudgetDigest
        << "}";

    return oss.str();
}

utils::Amount ProtectionTreasury::treasuryBalanceFromGenesis(
    const config::GenesisConfig& genesisConfig
) {
    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot calculate genesis treasury balance from invalid genesis config.");
    }

    utils::Amount balance;

    for (const config::GenesisAccountConfig& account : genesisConfig.genesisAccounts()) {
        if (account.address() == TREASURY_ADDRESS) {
            balance = balance + account.balance();
        }
    }

    if (balance.isNegative()) {
        throw std::invalid_argument("Genesis treasury balance cannot be negative.");
    }

    return balance;
}

GenesisTreasurySnapshot ProtectionTreasury::buildGenesisTreasurySnapshot(
    const config::GenesisConfig& genesisConfig,
    std::uint64_t blockHeight
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build genesis treasury snapshot at genesis height.");
    }

    const utils::Amount balance =
        treasuryBalanceFromGenesis(genesisConfig);

    const utils::Amount protectionBudget =
        utils::Amount::fromRawUnits(
            basisPointAmount(
                balance.rawUnits(),
                NODO_TREASURY_PROTECTION_BUDGET_BASIS_POINTS
            )
        );

    return GenesisTreasurySnapshot(
        "ACTIVE",
        TREASURY_ADDRESS,
        blockHeight,
        balance,
        balance - protectionBudget,
        protectionBudget,
        balance,
        TREASURY_SNAPSHOT_REASON
    );
}

ProtectionRewardBudget ProtectionTreasury::buildProtectionRewardBudget(
    const GenesisTreasurySnapshot& treasurySnapshot,
    const std::vector<RewardDistribution>& rewardDistributions
) {
    if (!treasurySnapshot.active()) {
        throw std::invalid_argument("Cannot build protection reward budget from inactive treasury snapshot.");
    }

    const utils::Amount requested =
        RewardDistributionCalculator::totalReward(
            rewardDistributions
        );

    const utils::Amount planned =
        minAmount(
            requested,
            treasurySnapshot.protectionBudget()
        );

    const std::uint64_t beneficiaryCount =
        planned.isPositive() ? static_cast<std::uint64_t>(rewardDistributions.size()) : 0;

    return ProtectionRewardBudget(
        "ACTIVE",
        treasurySnapshot.blockHeight(),
        treasurySnapshot.treasuryAddress(),
        treasurySnapshot.protectionBudget(),
        planned,
        treasurySnapshot.protectionBudget() - planned,
        beneficiaryCount,
        PROTECTION_BUDGET_REASON,
        treasurySnapshot.serialize()
    );
}

std::vector<ProtectionRewardGrant> ProtectionTreasury::buildProtectionRewardGrants(
    const ProtectionRewardBudget& budget,
    const std::vector<RewardDistribution>& rewardDistributions,
    const std::vector<SecurityScoreRecord>& securityScoreRecords
) {
    if (!budget.active()) {
        throw std::invalid_argument("Cannot build protection reward grants from inactive budget.");
    }

    if (budget.plannedTotal().isZero()) {
        return {};
    }

    if (rewardDistributions.empty()) {
        throw std::invalid_argument("Cannot allocate protection rewards without beneficiaries.");
    }

    std::uint64_t totalWeight = 0;
    std::vector<std::uint16_t> weights;
    weights.reserve(rewardDistributions.size());

    for (const RewardDistribution& reward : rewardDistributions) {
        if (!reward.isValid()) {
            throw std::invalid_argument("Cannot allocate protection rewards from invalid reward distribution.");
        }

        const std::uint16_t score =
            scoreForValidator(
                securityScoreRecords,
                reward.validatorAddress()
            );

        weights.push_back(score);
        totalWeight += static_cast<std::uint64_t>(score);
    }

    if (totalWeight == 0) {
        throw std::invalid_argument("Cannot allocate protection rewards with zero weight.");
    }

    std::vector<ProtectionRewardGrant> grants;
    grants.reserve(rewardDistributions.size());

    std::int64_t allocated = 0;
    const std::int64_t plannedRaw =
        budget.plannedTotal().rawUnits();

    for (std::size_t index = 0; index < rewardDistributions.size(); ++index) {
        std::int64_t grantRaw = 0;

        if (index + 1 == rewardDistributions.size()) {
            grantRaw = plannedRaw - allocated;
        } else {
            const std::int64_t weight =
                static_cast<std::int64_t>(weights[index]);

            const std::int64_t divisor =
                static_cast<std::int64_t>(totalWeight);

            grantRaw =
                (plannedRaw / divisor) * weight +
                ((plannedRaw % divisor) * weight) / divisor;
        }

        allocated += grantRaw;

        if (grantRaw <= 0) {
            continue;
        }

        grants.emplace_back(
            rewardDistributions[index].validatorAddress(),
            budget.blockHeight(),
            utils::Amount::fromRawUnits(grantRaw),
            weights[index],
            PROTECTION_GRANT_REASON,
            budget.serialize()
        );
    }

    return grants;
}

bool ProtectionTreasury::sameTreasurySnapshot(
    const GenesisTreasurySnapshot& left,
    const GenesisTreasurySnapshot& right
) {
    return left.serialize() == right.serialize();
}

bool ProtectionTreasury::sameBudget(
    const ProtectionRewardBudget& left,
    const ProtectionRewardBudget& right
) {
    return left.serialize() == right.serialize();
}

bool ProtectionTreasury::sameGrants(
    const std::vector<ProtectionRewardGrant>& left,
    const std::vector<ProtectionRewardGrant>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::node
