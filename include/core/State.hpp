#ifndef NODO_CORE_STATE_HPP
#define NODO_CORE_STATE_HPP

#include "core/CoinLot.hpp"
#include "core/Transaction.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * State represents the current reconstructed network state.
 *
 * It stores:
 * - created coins;
 * - traceable coin lots;
 * - total minted supply;
 * - current block height;
 * - applied transaction ids.
 */
class State {
public:
    State();

    static const std::string& feePoolAddress();

    std::uint64_t currentBlockIndex() const;
    utils::Amount totalSupply() const;

    const std::vector<economics::MintRecord>& mintRecords() const;
    const std::vector<CoinLot>& coinLots() const;

    void advanceBlock();

    /*
     * Creates coins from a valid MintRecord.
     *
     * Security rule:
     * Every created coin must generate a traceable CoinLot.
     */
    void applyMintRecord(const economics::MintRecord& mintRecord);

    /*
     * Applies a transfer using spendable CoinLots.
     *
     * Security rules:
     * - locked lots cannot be spent;
     * - spent lots cannot be spent again;
     * - transfer outputs preserve origin MintRecord ids;
     * - fee is preserved as a fee-pool CoinLot;
     * - duplicate transaction ids are rejected.
     */
    void applyTransferTransaction(const Transaction& transaction);

    /*
     * Locks a CoinLot for economic security.
     */
    void lockCoinLotForSecurity(
        const std::string& coinLotId,
        std::uint64_t lockedUntilBlock
    );

    utils::Amount balanceOf(const std::string& ownerAddress) const;

    std::uint64_t totalSecurityWeight() const;

    bool isTransactionAlreadyApplied(const std::string& transactionId) const;

    /*
     * Verifies that minted supply is still represented by active CoinLots.
     *
     * This is intentionally strict for the current phase.
     * Future burn and slashing policies will extend this rule.
     */
    bool isSupplyAuditable() const;

private:
    std::uint64_t m_currentBlockIndex;
    utils::Amount m_totalSupply;
    std::vector<economics::MintRecord> m_mintRecords;
    std::vector<CoinLot> m_coinLots;
    std::vector<std::string> m_appliedTransactionIds;

    std::string createCoinLotIdFromMint(const economics::MintRecord& mintRecord) const;

    std::string createTransferOutputCoinLotId(
        const Transaction& transaction,
        const CoinLot& inputLot,
        const std::string& outputKind,
        std::size_t outputIndex
    ) const;
};

} // namespace nodo::core

#endif