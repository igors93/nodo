#ifndef NODO_CORE_STATE_HPP
#define NODO_CORE_STATE_HPP

#include "core/Account.hpp"
#include "core/CoinLot.hpp"
#include "core/CoinLotRegistry.hpp"
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
 * - accounts and replay-protection nonces;
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

    const std::vector<Account>& accounts() const;
    const std::vector<economics::MintRecord>& mintRecords() const;
    const std::vector<CoinLot>& coinLots() const;

    /*
     * Builds a registry view from the currently reconstructed coin lots.
     *
     * Security rule:
     * The registry is a view derived from State, not a separate source of truth.
     */
    CoinLotRegistry coinLotRegistry() const;

    bool hasAccount(const std::string& address) const;
    std::uint64_t nextNonceOf(const std::string& address) const;

    void advanceBlock();

    /*
     * Creates coins from a valid MintRecord.
     *
     * Security rule:
     * Every created coin must generate a traceable CoinLot.
     * The recipient account is created deterministically if it does not exist.
     */
    void applyMintRecord(const economics::MintRecord& mintRecord);

    /*
     * Applies a transfer using CoinLotRegistry-backed validation.
     *
     * This method now delegates the security-critical lot selection and lot
     * movement to CoinLotTransactionValidator.
     */
    void applyTransferTransaction(const Transaction& transaction);

    /*
     * Explicit registry-backed transfer entrypoint.
     *
     * Kept separate so tests and future migration code can call the new path
     * directly while applyTransferTransaction remains backwards compatible.
     */
    void applyTransferTransactionUsingRegistry(const Transaction& transaction);

    void lockCoinLotForSecurity(
        const std::string& coinLotId,
        std::uint64_t lockedUntilBlock
    );

    utils::Amount balanceOf(const std::string& ownerAddress) const;

    std::uint64_t totalSecurityWeight() const;

    bool isTransactionAlreadyApplied(const std::string& transactionId) const;

    bool isSupplyAuditable() const;

private:
    std::uint64_t m_currentBlockIndex;
    utils::Amount m_totalSupply;
    std::vector<Account> m_accounts;
    std::vector<economics::MintRecord> m_mintRecords;
    std::vector<CoinLot> m_coinLots;
    std::vector<std::string> m_appliedTransactionIds;

    Account* findAccount(const std::string& address);
    const Account* findAccount(const std::string& address) const;

    void ensureAccountExists(const std::string& address);

    std::string createCoinLotIdFromMint(const economics::MintRecord& mintRecord) const;

    void replaceCoinLotsFromRegistry(
        const CoinLotRegistry& registry
    );
};

} // namespace nodo::core

#endif
