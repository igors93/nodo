#ifndef NODO_CORE_LEDGER_RECORD_HPP
#define NODO_CORE_LEDGER_RECORD_HPP

#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "economics/MintRecord.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * LedgerRecordType defines what kind of event was accepted into the ledger.
 *
 * A LedgerRecord is not the same as a Transaction.
 * A Transaction is a request.
 * A LedgerRecord is an accepted and auditable ledger entry.
 */
enum class LedgerRecordType {
    MINT,
    TRANSACTION
};

std::string ledgerRecordTypeToString(LedgerRecordType type);

/*
 * LedgerRecord represents an immutable record accepted by the Nodo ledger.
 *
 * Security principle:
 * Important events should not be applied silently.
 * They should become deterministic, hashable, auditable records.
 */
class LedgerRecord {
public:
    static LedgerRecord fromMintRecord(
        const economics::MintRecord& mintRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromTransaction(
        const Transaction& transaction,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        std::int64_t timestamp
    );

    const std::string& id() const;
    LedgerRecordType type() const;
    const std::string& sourceId() const;
    const std::string& payload() const;
    const std::string& payloadHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    /*
     * Deterministic serialization.
     *
     * This representation will later be used inside Block hashing.
     */
    std::string serialize() const;

private:
    LedgerRecord(
        std::string id,
        LedgerRecordType type,
        std::string sourceId,
        std::string payload,
        std::string payloadHash,
        std::int64_t timestamp
    );

    static std::string hashPayload(const std::string& payload);

    static std::string computeRecordId(
        LedgerRecordType type,
        const std::string& sourceId,
        const std::string& payloadHash,
        std::int64_t timestamp
    );

    std::string m_id;
    LedgerRecordType m_type;
    std::string m_sourceId;
    std::string m_payload;
    std::string m_payloadHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::core

#endif