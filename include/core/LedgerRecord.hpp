#ifndef NODO_CORE_LEDGER_RECORD_HPP
#define NODO_CORE_LEDGER_RECORD_HPP

#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/MintRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorPenaltyRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "privacy/PrivateAccountingRecord.hpp"

#include <cstdint>
#include <string>

namespace nodo::serialization {
class LedgerRecordCodec;
}

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
    TRANSACTION,
    PRIVATE_ACCOUNTING,
    VALIDATION_WORK,
    VALIDATOR_SCORE,
    PROTECTION_EPOCH,
    GENESIS_REWARD,
    VALIDATOR_PENALTY
};

std::string ledgerRecordTypeToString(LedgerRecordType type);

LedgerRecordType ledgerRecordTypeFromString(
    const std::string& value
);

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

    static LedgerRecord fromPrivateAccountingRecord(
        const privacy::PrivateAccountingRecord& privateAccountingRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromValidationWorkRecord(
        const economics::ValidationWorkRecord& validationWorkRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromValidatorScoreRecord(
        const economics::ValidatorScoreRecord& validatorScoreRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromProtectionEpoch(
        const economics::ProtectionEpoch& protectionEpoch,
        std::int64_t timestamp
    );

    static LedgerRecord fromGenesisRewardRecord(
        const economics::GenesisRewardRecord& genesisRewardRecord,
        std::int64_t timestamp
    );

    static LedgerRecord fromValidatorPenaltyRecord(
        const economics::ValidatorPenaltyRecord& validatorPenaltyRecord,
        std::int64_t timestamp
    );

    /*
     * Rebuilds an already-accepted ledger record from durable block storage.
     *
     * This does not re-run the original event admission flow. It verifies that
     * the stored id/payloadHash/timestamp still form a valid immutable record.
     */
    static LedgerRecord fromPersistedFields(
        std::string id,
        LedgerRecordType type,
        std::string sourceId,
        std::string payload,
        std::string payloadHash,
        std::int64_t timestamp
    );

    const std::string& id() const;
    LedgerRecordType type() const;
    const std::string& sourceId() const;
    const std::string& payload() const;
    const std::string& payloadHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    std::string serialize() const;

private:
    friend class nodo::serialization::LedgerRecordCodec;

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

    static std::string computeDerivedSourceId(
        const std::string& sourcePrefix,
        const std::string& payloadHash
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
