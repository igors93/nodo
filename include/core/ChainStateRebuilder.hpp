#ifndef NODO_CORE_CHAIN_STATE_REBUILDER_HPP
#define NODO_CORE_CHAIN_STATE_REBUILDER_HPP

#include "core/Blockchain.hpp"
#include "core/State.hpp"
#include "core/StateTransitionPreviewContext.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace nodo::core {

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
    std::size_t protectionMetadataRecordCount() const;
    std::size_t validatorPenaltyRecordCount() const;

    // Engine-commitment verification results.
    bool commitmentVerificationPassed() const;
    std::uint64_t firstFailedCommitmentHeight() const;
    const std::string& commitmentFailureReason() const;

    void markFailure(std::string reason);

    void setBlockCount(std::size_t value);
    void incrementLedgerRecordCount();
    void incrementMintRecordCount();
    void incrementGenesisRewardRecordCount();
    void incrementTransactionRecordCount();
    void incrementProtectionMetadataRecordCount();
    void incrementValidatorPenaltyRecordCount();
    void setCommitmentVerificationPassed(bool value);
    void setFirstFailedCommitmentHeight(std::uint64_t height);
    void setCommitmentFailureReason(std::string reason);

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    std::size_t m_blockCount;
    std::size_t m_ledgerRecordCount;
    std::size_t m_mintRecordCount;
    std::size_t m_genesisRewardRecordCount;
    std::size_t m_transactionRecordCount;
    std::size_t m_protectionMetadataRecordCount;
    std::size_t m_validatorPenaltyRecordCount;

    bool m_commitmentVerificationPassed;
    std::uint64_t m_firstFailedCommitmentHeight;
    std::string m_commitmentFailureReason;
};

class ChainStateRebuilder {
public:
    static StateRebuildReport auditBlockchain(const Blockchain& blockchain);

    static State rebuildStateFromGenesisRewardRecords(const Blockchain& blockchain);

    static State rebuildStateFromLedgerRecords(const Blockchain& blockchain);

    // Re-executes every non-genesis block through StateTransitionEngine and
    // compares the engine-computed stateRoot and receiptsRoot to the block's
    // declared values. Returns a StateRebuildReport with commitmentVerificationPassed()
    // set; the report also records the first failing block height on mismatch.
    // The contextBuilder receives the partial blockchain (all prior blocks) so it
    // can build the correct account state for each candidate block.
    static StateRebuildReport rebuildAndVerifyViaEngine(
        const Blockchain& blockchain,
        std::function<StateTransitionPreviewContext(const Blockchain&)> contextBuilder
    );
};

} // namespace nodo::core

#endif
