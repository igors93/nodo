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
    std::size_t validatorPenaltyRecordCount() const;

    void markFailure(std::string reason);

    void setBlockCount(std::size_t value);
    void incrementLedgerRecordCount();
    void incrementMintRecordCount();
    void incrementGenesisRewardRecordCount();
    void incrementTransactionRecordCount();
    void incrementPrivateAccountingRecordCount();
    void incrementProtectionMetadataRecordCount();
    void incrementValidatorPenaltyRecordCount();

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
    std::size_t m_validatorPenaltyRecordCount;
};

class ChainStateRebuilder {
public:
    static StateRebuildReport auditBlockchain(const Blockchain& blockchain);

    static State rebuildStateFromMintRecords(const Blockchain& blockchain);

    static State rebuildStateFromGenesisRewardRecords(const Blockchain& blockchain);

    static State rebuildStateFromLedgerRecords(const Blockchain& blockchain);
};

} // namespace nodo::core

#endif
