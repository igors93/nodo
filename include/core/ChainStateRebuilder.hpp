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
 * Later, this report can include balances, rejected records, supply totals,
 * validator state, and private accounting audit data.
 */
class StateRebuildReport {
public:
    StateRebuildReport();

    bool success() const;
    const std::string& failureReason() const;

    std::size_t blockCount() const;
    std::size_t ledgerRecordCount() const;
    std::size_t mintRecordCount() const;
    std::size_t transactionRecordCount() const;
    std::size_t privateAccountingRecordCount() const;

    void markFailure(std::string reason);

    void setBlockCount(std::size_t value);
    void incrementLedgerRecordCount();
    void incrementMintRecordCount();
    void incrementTransactionRecordCount();
    void incrementPrivateAccountingRecordCount();

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    std::size_t m_blockCount;
    std::size_t m_ledgerRecordCount;
    std::size_t m_mintRecordCount;
    std::size_t m_transactionRecordCount;
    std::size_t m_privateAccountingRecordCount;
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
     * Rebuilds State using only MINT LedgerRecords.
     *
     * This method remains useful for focused supply tests.
     */
    static State rebuildStateFromMintRecords(const Blockchain& blockchain);

    /*
     * Rebuilds public State from supported LedgerRecords.
     *
     * Supported in this phase:
     * - MINT
     * - TRANSFER
     * - PRIVATE_ACCOUNTING as public-state no-op
     *
     * Private accounting records are validated by the private accounting
     * subsystem. They do not directly mutate the public State.
     */
    static State rebuildStateFromLedgerRecords(const Blockchain& blockchain);
};

} // namespace nodo::core

#endif