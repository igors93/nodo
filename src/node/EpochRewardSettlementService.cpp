#include "node/EpochRewardSettlementService.hpp"

#include <sstream>

namespace nodo::node {

EpochRewardSettlement::EpochRewardSettlement()
    : m_totalMinted(utils::Amount())
    , m_settledAtBlock(0)
{}

EpochRewardSettlement::EpochRewardSettlement(
    std::vector<RewardDistribution> distributions,
    core::AccountStateView          updatedAccounts,
    utils::Amount                   totalMinted,
    std::uint64_t                   settledAtBlock
)
    : m_distributions(std::move(distributions))
    , m_updatedAccounts(std::move(updatedAccounts))
    , m_totalMinted(totalMinted)
    , m_settledAtBlock(settledAtBlock)
{}

const std::vector<RewardDistribution>& EpochRewardSettlement::distributions()   const { return m_distributions; }
const core::AccountStateView&          EpochRewardSettlement::updatedAccounts() const { return m_updatedAccounts; }
utils::Amount                          EpochRewardSettlement::totalMinted()     const { return m_totalMinted; }
std::uint64_t                          EpochRewardSettlement::settledAtBlock()  const { return m_settledAtBlock; }
bool                                   EpochRewardSettlement::isEmpty()         const { return m_distributions.empty(); }

bool EpochRewardSettlement::isValid() const {
    if (m_settledAtBlock == 0) return false;

    utils::Amount sum = utils::Amount();
    for (const auto& d : m_distributions) {
        if (!d.isValid()) return false;
        sum = utils::Amount::fromRawUnits(
            sum.rawUnits() + d.liquidReward().rawUnits()
        );
    }
    return sum.rawUnits() == m_totalMinted.rawUnits();
}

std::string EpochRewardSettlement::serialize() const {
    std::ostringstream oss;
    oss << "EpochRewardSettlement{"
        << "settledAtBlock=" << m_settledAtBlock
        << ";distributions=" << m_distributions.size()
        << ";totalMinted=" << m_totalMinted.rawUnits()
        << "}";
    return oss.str();
}

core::AccountStateView EpochRewardSettlementService::applyDistributions(
    const std::vector<RewardDistribution>& distributions,
    const core::AccountStateView&          accounts
) {
    core::AccountStateView updated = accounts;

    for (const auto& dist : distributions) {
        if (!dist.isValid() || dist.liquidReward().rawUnits() <= 0) continue;

        const core::AccountState current =
            updated.accountOrDefault(dist.validatorAddress());

        const core::AccountState credited(
            dist.validatorAddress(),
            utils::Amount::fromRawUnits(
                current.balance().rawUnits() + dist.liquidReward().rawUnits()
            ),
            current.nonce()
        );
        updated.putAccount(credited);
    }

    return updated;
}

EpochRewardSettlement EpochRewardSettlementService::settle(
    const consensus::QuorumCertificate& certificate,
    std::uint64_t                       blockHeight,
    utils::Amount                       feePool,
    utils::Amount                       baseBlockReward,
    const core::AccountStateView&       currentAccounts
) {
    const utils::Amount totalReward = utils::Amount::fromRawUnits(
        feePool.rawUnits() + baseBlockReward.rawUnits()
    );

    // Distribute equally across all certificate signers.
    const std::vector<RewardDistribution> distributions =
        RewardDistributionCalculator::buildFromQuorumCertificate(
            totalReward,
            certificate,
            blockHeight
        );

    if (distributions.empty()) {
        return EpochRewardSettlement(
            {},
            currentAccounts,
            utils::Amount(),
            blockHeight
        );
    }

    const core::AccountStateView updated =
        applyDistributions(distributions, currentAccounts);

    const utils::Amount totalLiquid =
        RewardDistributionCalculator::totalReward(distributions);

    return EpochRewardSettlement(
        distributions,
        updated,
        totalLiquid,
        blockHeight
    );
}

} // namespace nodo::node
