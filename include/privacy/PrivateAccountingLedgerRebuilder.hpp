#ifndef NODO_PRIVACY_PRIVATE_ACCOUNTING_LEDGER_REBUILDER_HPP
#define NODO_PRIVACY_PRIVATE_ACCOUNTING_LEDGER_REBUILDER_HPP

#include "core/Blockchain.hpp"
#include "privacy/PrivateAccountingLedger.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <string>

namespace nodo::privacy {

/*
 * PrivateAccountingLedgerRebuildReport summarizes the result of rebuilding
 * private accounting state from public blockchain history.
 *
 * Security principle:
 * A node must be able to independently rebuild private accounting metadata
 * from accepted chain records.
 */
class PrivateAccountingLedgerRebuildReport {
public:
    PrivateAccountingLedgerRebuildReport();

    bool success() const;
    const std::string& failureReason() const;

    std::size_t blockCount() const;
    std::size_t ledgerRecordCount() const;
    std::size_t privateAccountingRecordCount() const;
    std::size_t acceptedPrivateRecordCount() const;
    std::size_t nullifierCount() const;
    std::size_t commitmentCount() const;

    utils::Amount privateMintedSupply() const;
    utils::Amount privateBurnedSupply() const;
    utils::Amount outstandingPrivateSupply() const;

    void markFailure(std::string reason);
    void setBlockCount(std::size_t value);
    void incrementLedgerRecordCount();
    void incrementPrivateAccountingRecordCount();
    void setLedgerMetrics(const PrivateAccountingLedger& ledger);

    std::string serialize() const;

private:
    bool m_success;
    std::string m_failureReason;

    std::size_t m_blockCount;
    std::size_t m_ledgerRecordCount;
    std::size_t m_privateAccountingRecordCount;
    std::size_t m_acceptedPrivateRecordCount;
    std::size_t m_nullifierCount;
    std::size_t m_commitmentCount;

    utils::Amount m_privateMintedSupply;
    utils::Amount m_privateBurnedSupply;
    utils::Amount m_outstandingPrivateSupply;
};

/*
 * PrivateAccountingLedgerRebuilder rebuilds private accounting metadata from
 * official blockchain records.
 *
 * Important:
 * This does not verify production zero-knowledge proofs yet.
 * It rebuilds and validates the current development private accounting model.
 */
class PrivateAccountingLedgerRebuilder {
public:
    static PrivateAccountingLedgerRebuildReport auditBlockchain(
        const core::Blockchain& blockchain
    );

    static PrivateAccountingLedger rebuildFromBlockchain(
        const core::Blockchain& blockchain
    );
};

} // namespace nodo::privacy

#endif