#ifndef NODO_PRIVACY_PRIVATE_ACCOUNTING_LEDGER_HPP
#define NODO_PRIVACY_PRIVATE_ACCOUNTING_LEDGER_HPP

#include "privacy/NullifierSet.hpp"
#include "privacy/PrivateAccountingRecord.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::privacy {

/*
 * PrivateAccountingLedger stores accepted private accounting records.
 *
 * Security principle:
 * Private accounting must still be globally auditable.
 *
 * The ledger does not need to reveal owners or amounts in the future, but it
 * must enforce public safety rules:
 * - records must be valid;
 * - nullifiers cannot be reused;
 * - output commitments cannot be duplicated;
 * - private supply effects must remain auditable.
 */
class PrivateAccountingLedger {
public:
    PrivateAccountingLedger();

    std::size_t size() const;
    bool empty() const;

    const std::vector<PrivateAccountingRecord>& records() const;
    const NullifierSet& nullifierSet() const;

    utils::Amount privateMintedSupply() const;
    utils::Amount privateBurnedSupply() const;
    utils::Amount outstandingPrivateSupply() const;

    std::size_t registeredCommitmentCount() const;

    bool containsRecordId(const std::string& recordId) const;
    bool containsCommitmentId(const std::string& commitmentId) const;

    /*
     * Returns true only if the record can be safely appended.
     */
    bool canAppendRecord(const PrivateAccountingRecord& record) const;

    /*
     * Appends a private accounting record.
     *
     * Security rule:
     * Once a nullifier is accepted, it must never be accepted again.
     */
    void addRecord(const PrivateAccountingRecord& record);

    /*
     * Validates the full private accounting ledger.
     *
     * This is intentionally strict. If one record breaks accounting safety,
     * the whole private ledger must be considered invalid.
     */
    bool isValid() const;

    std::string serialize() const;

private:
    std::vector<PrivateAccountingRecord> m_records;
    NullifierSet m_nullifierSet;
    std::vector<std::string> m_registeredCommitmentIds;
    utils::Amount m_privateMintedSupply;
    utils::Amount m_privateBurnedSupply;

    bool hasDuplicateRecordIds() const;
    bool hasDuplicateCommitmentIds() const;

    bool recordOutputCommitmentsAreNew(
        const PrivateAccountingRecord& record
    ) const;

    bool recordInputNullifiersAreNew(
        const PrivateAccountingRecord& record
    ) const;

    void registerRecordInputsAndOutputs(
        const PrivateAccountingRecord& record
    );

    void applySupplyEffect(
        const PrivateAccountingRecord& record
    );
};

} // namespace nodo::privacy

#endif