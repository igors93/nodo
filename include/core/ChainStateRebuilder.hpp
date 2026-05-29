#ifndef NODO_CORE_CHAIN_STATE_REBUILDER_HPP
#define NODO_CORE_CHAIN_STATE_REBUILDER_HPP

#include "core/Blockchain.hpp"
#include "core/State.hpp"

#include <cstddef>
#include <string>

namespace nodo::core {

/*
 * StateRebuildReport summarizes the result of scanning a Blockchain.
 *
 * It intentionally counts both legacy and protection-economics records so the
 * migration from demo minting to GenesisReward can be audited.
 */
class StateRebuildReport {
public:
    StateRebuildReport();

    bool success() const;
    const std::string& failureReason() const;

    std::size_t blockCount() const;
    std::size_t ledgerRecordCount() const;
    std::size_t mintRecordCount() const;
    std::size_t genesisRewardRecordCount() const;
    std::size_t transactionRecordCount() const;
    std::size_t privateAccountingRecordCount() const;
    std::size_t protectionMetadataRecordCount() const;

    void markFailure(std::string reason);

    void setBlockCount(std::size_t value);
    void incrementLedgerRecordCount();
    void incrementMintRecordCount();
    void incrementGenesisRewardRecordCount();
    void incrementTransactionRecordCount();
    void incrementPrivateAccountingRecordCount();
    void incrementProtectionMetadataRecordCount();

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    std::size_t m_blockCount;
    std::size_t m_ledgerRecordCount;
    std::size_t m_mintRecordCount;
    std::size_t m_genesisRewardRecordCount;
    std::size_t m_transactionRecordCount;
    std::size_t m_privateAccountingRecordCount;
    std::size_t m_protectionMetadataRecordCount;
};

/*
 * ChainStateRebuilder scans the Blockchain from genesis to latest block.
 *
 * Security principle:
 * The current state must not be blindly trusted. A node should be able
 * to rebuild or audit the state from the accepted chain history.
 */
class ChainStateRebuilder {
public:
    static StateRebuildReport auditBlockchain(const Blockchain& blockchain);

    /*
     * Rebuilds State using only legacy MINT LedgerRecords.
     *
     * This remains useful for compatibility tests during the migration.
     */
    static State rebuildStateFromMintRecords(const Blockchain& blockchain);

    /*
     * Rebuilds State using only GENESIS_REWARD LedgerRecords.
     *
     * This is the new production-oriented supply creation path.
     */
    static State rebuildStateFromGenesisRewardRecords(const Blockchain& blockchain);

    /*
     * Rebuilds public State from supported LedgerRecords.
     *
     * Supported public mutations:
     * - MINT as legacy development supply creation
     * - GENESIS_REWARD as protection-economics supply creation
     * - TRANSACTION as transfer movement
     *
     * Supported public no-ops:
     * - PRIVATE_ACCOUNTING
     * - VALIDATION_WORK
     * - VALIDATOR_SCORE
     * - PROTECTION_EPOCH
     */
    static State rebuildStateFromLedgerRecords(const Blockchain& blockchain);
};

} // namespace nodo::core

#endif
