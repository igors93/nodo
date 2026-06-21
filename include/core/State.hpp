#ifndef NODO_CORE_STATE_HPP
#define NODO_CORE_STATE_HPP

#include "core/Account.hpp"
#include "core/AccountStateView.hpp"
#include "core/CoinLot.hpp"
#include "core/CoinLotRegistry.hpp"
#include "core/Transaction.hpp"
#include "economics/GenesisRewardRecord.hpp"
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
 * - traceable coin lots;
 * - legacy development mint records;
 * - GenesisReward records;
 * - total public supply;
 * - current block height;
 * - applied transaction ids.
 *
 * Security principle:
 * State is a rebuildable view. The chain history remains the source of truth.
 */
class State {
public:
    State();

    static const std::string& feePoolAddress();

    std::uint64_t currentBlockIndex() const;
    utils::Amount totalSupply() const;

    const std::vector<Account>& accounts() const;
    const std::vector<economics::MintRecord>& mintRecords() const;
    const std::vector<economics::GenesisRewardRecord>& genesisRewardRecords() const;
    const std::vector<CoinLot>& coinLots() const;

    /*
     * Builds the account-state view used by state-root verification.
     *
     * Balances are derived from CoinLots, while nonces come from Account replay
     * state. This keeps snapshot roots aligned with runtime validation.
     */
    AccountStateView accountStateView() const;

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
     * Legacy development coin creation.
     *
     * This remains available for compatibility with old tests and demo code, but
     * the production-oriented path should use applyGenesisRewardRecord().
     */
    void applyMintRecord(const economics::MintRecord& mintRecord);

    /*
     * Creates coins from a valid GenesisRewardRecord.
     *
     * Security rule:
     * New reward supply must enter State through a deterministic CoinLot created
     * by GenesisRewardRecord::createRewardCoinLot().
     */
    void applyGenesisRewardRecord(
        const economics::GenesisRewardRecord& genesisRewardRecord
    );

    /*
     * Applies a transfer using CoinLotRegistry-backed validation.
     *
     * This method delegates the security-critical lot selection and lot movement
     * to CoinLotTransactionValidator.
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

    /*
     * Audits public supply by comparing active CoinLots against all accepted
     * public supply-creation records.
     *
     * Both legacy MintRecord and new GenesisRewardRecord are counted so the
     * migration can happen safely without breaking existing chain fixtures.
     */
    bool isSupplyAuditable() const;

    std::string serialize() const;

    static State deserialize(
        const std::string& serialized
    );

private:
    std::uint64_t m_currentBlockIndex;
    utils::Amount m_totalSupply;
    std::vector<Account> m_accounts;
    std::vector<economics::MintRecord> m_mintRecords;
    std::vector<economics::GenesisRewardRecord> m_genesisRewardRecords;
    std::vector<CoinLot> m_coinLots;
    std::vector<std::string> m_appliedTransactionIds;

    Account* findAccount(const std::string& address);
    const Account* findAccount(const std::string& address) const;

    void ensureAccountExists(const std::string& address);

    std::string createCoinLotIdFromMint(const economics::MintRecord& mintRecord) const;

    bool hasLegacyMintRecord(const std::string& mintRecordId) const;
    bool hasGenesisRewardRecord(const std::string& genesisRewardId) const;

    void replaceCoinLotsFromRegistry(
        const CoinLotRegistry& registry
    );
};

} // namespace nodo::core

#endif
